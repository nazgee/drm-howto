/*
 * plus.cpp
 *
 *  Created on: Dec 8, 2015
 *      Author: nazgee
 */

//#include <epoxy/gl.h>
//#include <epoxy/egl.h>
//
//
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#define virtual myvirtual
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm.h>
#undef virtual

#include "Exception.h"
#include "Config.h"

#define LOGVV(fmt, ...) fprintf(stdout, fmt "\n", ##__VA_ARGS__)
#define LOGVD(fmt, ...) fprintf(stdout, fmt "\n", ##__VA_ARGS__)
#define LOGVE(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)

class IUncopyable {
public:
    IUncopyable() {
    }

private:
    IUncopyable(const IUncopyable&);
    IUncopyable& operator=(const IUncopyable&);
};

//========================================================
class DRM_Resources: public IUncopyable {
    drmModeRes *res;
public:
    DRM_Resources(drmModeRes *res) :
            res(res) {
        if (!res) {
            throw Exception("drmModeGetResources failed");
        }
    }
    ~DRM_Resources() {
        drmModeFreeResources(res);
    }

    uint32_t getConnectorsCount() {
        return res->count_connectors;
    }

    uint32_t getCrtcsCount() {
        return res->count_crtcs;
    }

    uint32_t getEncodersCount() {
        return res->count_encoders;
    }

    uint32_t getFbsCount() {
        return res->count_fbs;
    }

    uint32_t getConnectorId(uint32_t connector_index) {
        if (connector_index >= res->count_connectors) {
            throw Exception("got connector_index >= count_connectors");
        }
        return res->connectors[connector_index];
    }

    uint32_t getCrtcId(uint32_t crtc_index) {
        if (crtc_index >= res->count_crtcs) {
            throw Exception("got crtc_index >= count_crtcs");
        }
        return res->crtcs[crtc_index];
    }

    uint32_t getEncoderId(uint32_t encoder_index) {
        if (encoder_index >= res->count_encoders) {
            throw Exception("got encoder_index >= count_encoders");
        }
        return res->encoders[encoder_index];
    }

    uint32_t getFbId(uint32_t fb_index) {
        if (fb_index >= res->count_fbs) {
            throw Exception("got fb_index >= count_fbs");
        }
        return res->fbs[fb_index];
    }
};
//========================================================
class DRM_Encoder: public IUncopyable {
    drmModeEncoder *mEncoder;
public:
    DRM_Encoder(drmModeEncoder *encoder) :
            mEncoder(encoder) {

    }

    ~DRM_Encoder() {
        drmModeFreeEncoder(mEncoder);
    }

    uint32_t getCrtcId() {
        return mEncoder->crtc_id;
    }

    bool isCrtcPossible(uint32_t crtc_index) {
        return (mEncoder->possible_crtcs & (1 << crtc_index));
    }

};
//========================================================
class DRM_Crtc: public IUncopyable {
    uint32_t crtc_id;
public:
    DRM_Crtc(uint32_t crtc_id) :
            crtc_id(crtc_id) {
    }

    ~DRM_Crtc() {

    }
};

//========================================================
class DRM_DumbBuffer: public IUncopyable {
    drm_mode_create_dumb creq;
    int fd;

public:
    DRM_DumbBuffer(int fd, uint32_t w, uint32_t h, uint8_t bpp) :
            fd(fd) {
        memset(&creq, 0, sizeof(creq));
        creq.width = w;
        creq.height = h;
        creq.bpp = bpp;
        int ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
        if (ret < 0) {
            LOGVE("cannot create dumb buffer (%d): %m", errno);
            throw Exception("cannot create dumb buffer:" + ret);
        }

        LOGVD("allocated dumb buffer %dx%d@%dbpp; handle=%d", getWidth(), getHeight(), getBPP(), getHandle());
    }

    ~DRM_DumbBuffer() {
        //destroy();
    }

    void destroy() {
        destroy(fd, getHandle());
    }

    static void destroy(int fd, uint32_t handle) {
        struct drm_mode_destroy_dumb dreq;
        memset(&dreq, 0, sizeof(dreq));
        dreq.handle = handle;
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
    }

//    /* create framebuffer object for the dumb-buffer */
//    ret = drmModeAddFB(fd, dev->width, dev->height, 24, 32, dev->stride,
//               dev->handle, &dev->fb);
//    if (ret) {
//        fprintf(stderr, "cannot create framebuffer (%d): %m\n",
//            errno);
//        ret = -errno;
//        goto err_destroy;
//    }

    uint8_t *map() {
        struct drm_mode_map_dumb mreq;
        uint8_t *map_result;

        /* prepare buffer for memory mapping */
        memset(&mreq, 0, sizeof(mreq));
        mreq.handle = getHandle();
        int ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
        if (ret) {
            throw Exception("ioctl DRM_IOCTL_MODE_MAP_DUMB failed:" + ret);
        }

        /* perform actual memory mapping */
        map_result = (uint8_t*) mmap(0, getSize(), PROT_READ | PROT_WRITE,
        MAP_SHARED, fd, mreq.offset);
        if (map_result == MAP_FAILED) {
            throw Exception("mapping dumb buffer failed:" + errno);
        }

        /* clear the framebuffer to 0 */
        memset(map_result, 0, getSize());
        return map_result;
    }

    uint32_t getWidth() {
        return creq.width;
    }

    uint32_t getHeight() {
        return creq.height;
    }

    uint32_t getBPP() {
        return creq.bpp;
    }

    uint32_t getStride() {
        return creq.pitch;
    }

    uint64_t getSize() {
        return creq.size;
    }

    uint32_t getHandle() {
        return creq.handle;
    }

};
//========================================================
class DRM_Connector: public IUncopyable {
    bool mIsConnected;
    drmModeConnector *mConnector;
public:
    DRM_Connector(drmModeConnector *connector) :
            mConnector(connector) {

        if (!mConnector) {
            throw new Exception("null drmModeConnector given");
        }

        mIsConnected = mConnector->connection == DRM_MODE_CONNECTED;
        if (!isConnected()) {
            LOGVE("connector %d is not connected", getId());
            return;
        }

        if (getModesCount() <= 0) {
            LOGVE("connector %d has no modes available", getId());
            return;
        }

        LOGVD("connector %d max resolution is %dx%d", getId(), getWidth(), getHeight());
    }

    ~DRM_Connector() {
        drmModeFreeConnector(mConnector);
    }

    uint32_t getType() {
        return mConnector->connector_type;
    }

    uint32_t getId() {
        return mConnector->connector_id;
    }

    bool isConnected() {
        return mIsConnected;
    }

    uint32_t getModesCount() {
        return mConnector->count_modes;
    }

    drmModeModeInfo getMode(uint32_t mode_index = 0) {
        if (mode_index >= getModesCount()) {
            throw Exception("got mode >= count_modes");
        }
        return mConnector->modes[mode_index];
    }

    uint32_t getWidth(uint32_t mode_index = 0) {
        return mConnector->modes[mode_index].hdisplay;
    }

    uint32_t getHeight(uint32_t mode_index = 0) {
        return mConnector->modes[mode_index].vdisplay;
    }

    uint32_t getEncodersCount() {
        return mConnector->count_encoders;
    }

    uint32_t getEncoderId(uint32_t encoder_index) {
        if (encoder_index >= getEncodersCount()) {
            throw Exception("got encoder_index >= count_encoders");
        }
        return mConnector->encoders[encoder_index];
    }

    uint32_t getCurrentEncoderId() {
        return mConnector->encoder_id;
    }

    drmModeEncoder* getCurrentEncoder(int fd) {
        if (getCurrentEncoderId()) {
            return drmModeGetEncoder(fd, getCurrentEncoderId());
        }
        return NULL;
    }
};

//========================================================
class DeviceNode {
public:
    drmModeModeInfo mode;
    uint32_t fb;
    uint32_t conn;
    uint32_t connector_type;
    uint32_t crtc;

    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t size;
    uint32_t handle;
    uint8_t *map;

    DeviceNode() :
            fb(0), conn(0), crtc(0), width(0), height(0), stride(0), size(0), handle(0), map(NULL) {
        memset(&mode, 0, sizeof(mode));
    }

    void setBuffer(DRM_DumbBuffer &buffer) {
        map = buffer.map();
        size = buffer.getSize();
        stride = buffer.getStride();
        handle = buffer.getHandle();
    }

    void dumpself() {
        LOGVD("devnode: CRTC=%d conn=%d FB=%d %dx%d, buffer: %dB handle=%d map=%p", crtc, conn, fb, width, height, size,
                handle, map);
    }
};
//========================================================
class DRM {
    int fd;
    std::vector<DeviceNode> mAllocatedDevices;
public:
    DRM(const char name[]) :
            fd(-1) {
        LOGVD("opening DRM device");
        fd = drmOpen(name, NULL);

        if (!hasCap(DRM_CAP_DUMB_BUFFER)) {
            throw Exception("DRM_CAP_DUMB_BUFFER not available");
        }

        {
            // create device
            //DeviceNode device = findDeviceOnConnector(DRM_MODE_CONNECTOR_HDMIA);
            DeviceNode device = findDeviceOnConnector(3);

            // create data buffer
            int bpp = 32;
            DRM_DumbBuffer buffer(fd, device.width, device.height, bpp);

            // create framebuffer
            int ret = drmModeAddFB(fd, buffer.getWidth(), buffer.getHeight(), 24, bpp, buffer.getStride(),
                    buffer.getHandle(), &device.fb);
            if (ret) {
                buffer.destroy();
                throw Exception("drmModeAddFB failed for buffer:" + buffer.getHandle());
            }

            // register buffer in device
            device.setBuffer(buffer);

            // we're done
            mAllocatedDevices.push_back(device);
            device.dumpself();
        }

        {
            DeviceNode device = mAllocatedDevices[0];
            LOGVD("modesetting %d", device.conn);
            device.dumpself();

            drmModeCrtc *saved_crtc = drmModeGetCrtc(fd, device.crtc);
            if (drmModeSetCrtc(fd, device.crtc, device.fb, 0, 0, &device.conn, 1, &device.mode)) {
                LOGVE("cannot set CRTC for connector %u (%d): %m", device.conn, errno);
                throw Exception("cannot set crtc for DeviceNode");
            }
            LOGVD("set CRTC for connector %u, starting draw()", device.conn);

            draw();

            // restore saved CRTC configuration
            drmModeSetCrtc(fd, saved_crtc->crtc_id, saved_crtc->buffer_id, saved_crtc->x, saved_crtc->y, &device.conn,
                    1, &saved_crtc->mode);
            drmModeFreeCrtc(saved_crtc);

//             /* unmap buffer */
//             munmap(device.map, device.size);
//
//             /* delete framebuffer */
//             drmModeRmFB(fd, device.fb);
//
//             /* delete dumb buffer */
//             memset(&dreq, 0, sizeof(dreq));
//             dreq.handle = iter->handle;
//             drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
//
//             /* free allocated memory */
        }

    }

    void draw(void) {
        uint8_t checkboardmask = 0x07;
        uint8_t r, g, b;
        uint8_t R, G, B;
        bool r_up, g_up, b_up;
        unsigned int i, j, k, off;
        struct modeset_dev *iter;

        srand(time(NULL));
        r = rand() % 0xff;
        g = rand() % 0xff;
        b = rand() % 0xff;
        r_up = g_up = b_up = true;

        for (i = 0; i < 50; ++i) {
            r = next_color(&r_up, r, 20);
            g = next_color(&g_up, g, 10);
            b = next_color(&b_up, b, 5);

            for (unsigned int i = 0; i < mAllocatedDevices.size(); i++) {
                DeviceNode device = mAllocatedDevices[i];
                // TODO should use buffer's dimensions, not device
                for (unsigned int row = 0; row < device.height; row++) {
                    for (unsigned int col = 0; col < device.width; col++) {

                        uint8_t p = 0xff;
                        if ((col % 40 > 20 && row % 40 < 20) || (col % 40 < 20 && row % 40 > 20))
                            p = 0;

                        if (checkboardmask & 1) {
                            R = p ? p : r;
                        } else {
                            R = r;
                        }

                        if (checkboardmask & 2) {
                            G = p ? p : g;
                        } else {
                            G = g;
                        }

                        if (checkboardmask & 4) {
                            B = p ? p : b;
                        } else {
                            B = b;
                        }

                        off = device.stride * row + col * 4;
                        *(uint32_t*) &device.map[off] = (R << 16) | (G << 8) | B;
                    }
                }
            }

            usleep(10000);
        }
    }

    uint8_t next_color(bool *up, uint8_t cur, unsigned int mod) {
        uint8_t next;

        next = cur + (*up ? 1 : -1) * (rand() % mod);
        if ((*up && next < cur) || (!*up && next > cur)) {
            *up = !*up;
            next = cur;
        }

        return next;
    }

    ~DRM() {
        LOGVD("closing DRM device");
        drmClose(fd);
    }

private:

    bool hasCap(int capability) {
        uint64_t has_capability = 0;
        if (drmGetCap(fd, capability, &has_capability) < 0) {
            throw Exception("drmGetCap failed unexpectedly");
        }
        return !!has_capability;
    }

    bool isAnyDeviceUsingCRTC(uint32_t crtc_id) {
        for (int i = 0; i < mAllocatedDevices.size(); i++) {
            if (mAllocatedDevices[i].crtc == crtc_id) {
                return false;
            }
        }
        return false;
    }

    DeviceNode findDeviceOnConnector(uint32_t connector_type) {
        DeviceNode found_device;
        DRM_Resources res(drmModeGetResources(fd));
        for (unsigned int i = 0; i < res.getConnectorsCount(); ++i) {
            DeviceNode tmp_device;
            drmModeConnector *p_conn = drmModeGetConnector(fd, res.getConnectorId(i));
            if (!p_conn) {
                LOGVE("cannot retrieve DRM connector %u:%u (%d): %m", i, res.getConnectorId(i), errno);
                continue;
            }

            // get mode info for this connector
            DRM_Connector conn(p_conn);
            if (!conn.isConnected()) {
                LOGVD("nothing connected to connector %d - ignoring it", conn.getId());
                continue;
            }
            tmp_device.connector_type = conn.getType();
            tmp_device.conn = conn.getId();
            tmp_device.mode = conn.getMode();
            tmp_device.width = conn.getWidth();
            tmp_device.height = conn.getHeight();

            //find a CRTC for this connector
            int32_t crtc_id = findFreeDeviceOnCRTC(conn, res);
            if (crtc_id < 0) {
                LOGVE("no valid CRTC for connector %d", conn.getId());
                continue;
            }
            tmp_device.crtc = crtc_id;

            tmp_device.dumpself();
            if (tmp_device.connector_type == connector_type) {
                found_device = tmp_device;
                LOGVD("found device matching desired connector_type:%d", connector_type);
                break;
            } else {
                LOGVD("still looking for connector_type:%d, got:%d", connector_type, tmp_device.connector_type);
            }
        }

        return found_device;
    }

    int32_t findFreeDeviceOnCRTC(DRM_Connector& conn, DRM_Resources& res) {
        // try currently connected encoder+crtc
        drmModeEncoder* p_enc = conn.getCurrentEncoder(fd);
        if (p_enc) {
            DRM_Encoder enc(p_enc);
            uint32_t crtc = enc.getCrtcId();
            // TODO check if it should be '>' or '>='
            if (crtc > 0 && !isAnyDeviceUsingCRTC(crtc)) {
                return crtc;
            }
        } else {
            /* If the connector is not currently bound to an encoder or if
             the
             * encoder+crtc is already used by another connector (actually
             unlikely
             * but lets be safe), iterate all other available encoders to
             find a
             * matching CRTC. */
            for (unsigned int i = 0; i < conn.getEncodersCount(); ++i) {
                p_enc = drmModeGetEncoder(fd, conn.getEncoderId(i));
                if (!p_enc) {
                    LOGVE("cannot retrieve DRM encoder %u:%u (%d): %m\n", i, conn.getEncoderId(i), errno);
                    continue;
                }
                DRM_Encoder enc(p_enc);
                // iterate over globally available CRTCs
                for (unsigned int i = 0; i < res.getCrtcsCount(); ++i) {
                    if (!enc.isCrtcPossible(i)) {
                        continue;
                    }
                    uint32_t crtc = res.getCrtcId(i);
                    if (!isAnyDeviceUsingCRTC(crtc)) {
                        return crtc;
                    }
                }
            }
        }
        return -1;
    }
};

int main(int argc, char** argv) {
    Config::parse(argc, argv);
    DRM drm(Config::getDrmNodeName());
}

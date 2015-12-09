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

#define LOGVV(...) fprintf(stdout, __VA_ARGS__)
#define LOGVD(...) fprintf(stdout, __VA_ARGS__)
#define LOGVE(...) fprintf(stderr, __VA_ARGS__)

//========================================================
class DRM_Resources {
    drmModeRes *res;
public:
    DRM_Resources(drmModeRes * res) :
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
class DRM_Encoder {
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
class DRM_Crtc {
public:
    DRM_Crtc() {

    }

    ~DRM_Crtc() {

    }
};

//========================================================
class DRM_DumbBuffer {
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
            LOGVE("cannot create dumb buffer (%d): %m\n", errno);
            throw Exception("cannot create dumb buffer:" + ret);
        }

        LOGVD("allocated dumb buffer %dx%d@%dbpp; handle=%d\n", getWidth(), getHeight(), getBPP(), getHandle());
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
        map_result = (uint8_t*)mmap(0, getSize(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
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
class DRM_Connector {
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
            LOGVE("connector %d is not connected\n", getId());
            return;
        }

        if (getModesCount() <= 0) {
            LOGVE("connector %d has no modes available\n", getId());
            return;
        }

        LOGVD("connector %d max resolution is %dx%d\n", getId(), getWidth(), getHeight());
    }

    ~DRM_Connector() {
        drmModeFreeConnector(mConnector);
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

//     drmModeModeInfo mode;
//     uint32_t fb;
//     uint32_t conn;
//     uint32_t crtc;
//     drmModeCrtc *saved_crtc;
//
//     uint32_t width;
//     uint32_t height;
//     uint32_t stride;
//     uint32_t size;
//     uint32_t handle;
//     uint8_t *map;
};

//========================================================
struct devnode {
     drmModeModeInfo mode;
     uint32_t fb;
     uint32_t conn;
     uint32_t crtc;
     drmModeCrtc *saved_crtc;

     uint32_t width;
     uint32_t height;
     uint32_t stride;
     uint32_t size;
     uint32_t handle;
     uint8_t *map;

     devnode() :
         fb(0), conn(0), crtc(0), saved_crtc(NULL), width(0), height(0), stride(0), size(0), handle(0), map(NULL)
     {
         memset(&mode, 0, sizeof(mode));
     }

     void dumpself() {
         LOGVD("devnode: CRTC=%d conn=%d FB=%d, buffer: %dx%d@%dB handle=%d\n", crtc, conn, fb, width, height, size, handle);
     }
};
//========================================================
class DRM {
    int fd;
    std::vector<devnode> devices;
public:
    DRM(const char name[]) :
            fd(-1) {
        LOGVD("opening DRM device\n");
        fd = drmOpen(name, NULL);

        if (!hasCap(DRM_CAP_DUMB_BUFFER)) {
            throw Exception("DRM_CAP_DUMB_BUFFER not available");
        }

        prepare();
    }

    ~DRM() {
        LOGVD("closing DRM device\n");
        drmClose(fd);
    }

private:
    int32_t find_crtc_id(DRM_Connector& conn, DRM_Resources& res) {
        // try currently connected encoder+crtc
        drmModeEncoder* p_enc = conn.getCurrentEncoder(fd);
        if (p_enc) {
            DRM_Encoder enc(p_enc);
            uint32_t crtc = enc.getCrtcId();
            // TODO check if it should be '>' or '>='
            if (crtc > 0 && !isDeviceUsingCRTC(crtc)) {
                return crtc;
            }
        } else {
            /* If the connector is not currently bound to an encoder or if the
             * encoder+crtc is already used by another connector (actually unlikely
             * but lets be safe), iterate all other available encoders to find a
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
                    if (!isDeviceUsingCRTC(crtc)) {
                        return crtc;
                    }
                }
            }
        }
        return -1;
    }

    bool hasCap(int capability) {
        uint64_t has_capability = 0;
        if (drmGetCap(fd, capability, &has_capability) < 0) {
            throw Exception("drmGetCap failed unexpectedly\n");
        }
        return !!has_capability;
    }

    bool isDeviceUsingCRTC(uint32_t crtc_id) {
          // TODO
//        crtc = enc->crtc_id;
//        for (iter = modeset_list; iter; iter = iter->next) {
//            if (iter->crtc == crtc) {
//                crtc = -1;
//                break;
//            }
//        }
        return false;
    }

    void saveCRTC(uint32_t crtc_id) {
        // TODO dev->crtc = crtc;
    }

    void prepare() {
        DRM_Resources res(drmModeGetResources(fd));

        for (unsigned int i = 0; i < res.getConnectorsCount(); ++i) {
            struct devnode device;
            drmModeConnector *p_conn = drmModeGetConnector(fd, res.getConnectorId(i));
            if (!p_conn) {
                LOGVE("cannot retrieve DRM connector %u:%u (%d): %m\n", i, res.getConnectorId(i), errno);
                continue;
            }

            // get mode info for this connector
            DRM_Connector conn(p_conn);
            if (!conn.isConnected()) {
                LOGVD("nothing connected to connector %d - ignoring it\n", conn.getId());
                continue;
            }
            device.conn = conn.getId();
            device.mode = conn.getMode();
            device.width = conn.getWidth();
            device.height = conn.getHeight();

            //find a CRTC for this connector
            int32_t crtc_id = find_crtc_id(conn, res);
            if (crtc_id < 0) {
                LOGVE("no valid CRTC for connector %d\n", conn.getId());
                continue;
            }
            device.crtc = crtc_id;

            // create data buffer
            DRM_DumbBuffer buffer(fd, device.width, device.height, 32);
            // create framebuffer
            int ret = drmModeAddFB(fd, buffer.getWidth(), buffer.getHeight(), 24, buffer.getBPP(),
                    buffer.getStride(), buffer.getHandle(), &device.fb);
            if (ret) {
                LOGVE("drmModeAddFB failed for buffer (%d)\n", buffer.getHandle());
                buffer.destroy();
                continue;
            }
            // save buffer info
            device.map = buffer.map();
            device.size = buffer.getSize();
            device.stride = buffer.getStride();
            device.handle = buffer.getHandle();

            // store the device somewhere
            LOGVD("device ready for use!\n");
            device.dumpself();
        }
    }
};

int main(int argc, char** argv) {
    Config::parse(argc, argv);
    DRM drm(Config::getDrmNodeName());
}

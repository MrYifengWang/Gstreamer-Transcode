#pragma once

#include <functional>
#include <string>

#include <string>
#include <assert.h>
#include <pthread.h>
//#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

// gst
#include <gst/gst.h>
#include <gst/gstmemory.h>
#include <gst/gstpad.h>
#include <gst/allocators/gstdmabuf.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gstpipeline.h>
#include <gst/gstcaps.h>

#include <gst/video/gstvideometa.h>
#include <gst/base/gstbasesink.h>
#include <gst/video/video-info.h>
#include <gst/video/video-format.h>
#include <gst/video/video-enumtypes.h>
#include <gst/video/video-tile.h>
#include <gst/base/base-prelude.h>
#include <gst/gstquery.h>

#include <gst/audio/audio-info.h>
#include <gst/audio/audio-format.h>
#include <gst/audio/audio-enumtypes.h>
#include <gst/audio/gstaudiometa.h>

#include <gst/sdp/gstsdpmessage.h>
#include <gobject/gclosure.h>
#include <gst/rtsp/gstrtspmessage.h>

// gst
#include <gst/gst.h>
#include <gst/gstmemory.h>
#include <gst/gstpad.h>
#include <gst/allocators/gstdmabuf.h>
#include <gst/app/gstappsink.h>
#include <gst/gstpipeline.h>
#include <gst/gstcaps.h>

#include <gst/video/gstvideometa.h>
#include <gst/base/gstbasesink.h>
#include <gst/video/video-info.h>
#include <gst/video/video-format.h>
#include <gst/video/video-enumtypes.h>
#include <gst/video/video-tile.h>
#include <gst/base/base-prelude.h>
#include <gst/gstquery.h>

#include <gst/sdp/gstsdpmessage.h>
#include <gobject/gclosure.h>
#include <gobject/gsignal.h>
#include <gst/rtsp/gstrtspmessage.h>
#include <gst/rtsp/gstrtsptransport.h>

//rga
#define SRC_FORMAT RK_FORMAT_YCrCb_420_SP
#define DST_FORMAT RK_FORMAT_RGB_888

#define STATUS_INIT 0
#define STATUS_CONNECTED 1
#define STATUS_DISCONNECT 2
#define STATUS_CONNECTING 3

#define DEFAULT_CONN_MODE 0
#define TCP_CONN_MODE 1
#define UDP_CONN_MODE 2
class BaseGstTask
{
public:
	GMainLoop  *loop_ = NULL;
	GstElement *pipeline_ = NULL;
	GstBus *bus_ = NULL;
	gint format_;
	GstVideoInfo videoinfo_;
	GstAudioInfo audioinfo_;
	int tid_ = 0;
	int isRun = STATUS_INIT;
	char * sharebuf_ = NULL;
	char * shareAudiobuf_ = NULL;
	int shareAudioLen = 0;
	bool terminate = false;

};


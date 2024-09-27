#pragma once
#include "GstTask.h"
class GstUtil
{
public:
	static void print_pad_capabilities(GstElement *element, gchar *pad_name);
	static void print_pad_templates_information(GstElementFactory * factory);
	static void print_caps(const GstCaps * caps, const gchar * pfx);
	static gboolean print_field(GQuark field, const GValue * value, gpointer pfx);
	static gboolean check_media(GQuark field, const GValue * value, gpointer pfx);
	static int checkMediaPtype(GstPad *pad);
};


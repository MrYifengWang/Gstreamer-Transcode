#include "GstUtil.h"

/* Functions below print the Capabilities in a human-friendly format */
gboolean GstUtil::print_field(GQuark field, const GValue * value, gpointer pfx) {
	gchar *str = gst_value_serialize(value);

	g_print("%s  %15s sub: %s\n", (gchar *)pfx, g_quark_to_string(field), str);
	g_free(str);
	return TRUE;
}

gboolean GstUtil::check_media(GQuark field, const GValue * value, gpointer pfx) {
	gchar *str = gst_value_serialize(value);

	g_print("%s  %15s: %s\n", (gchar *)pfx, g_quark_to_string(field), str);
	g_free(str);
	return TRUE;
}

int GstUtil::checkMediaPtype(GstPad *pad)
{
	GstCaps *caps = NULL;
	int result = -1;
	/* Retrieve negotiated caps (or acceptable caps if negotiation is not finished yet) */
	caps = gst_pad_get_current_caps(pad);
	if (!caps)
		caps = gst_pad_query_caps(pad, NULL);

	/* Print and free */

	//print_caps(caps, "---media---");

	guint i;
	if (gst_caps_is_any(caps)) {
		return -1;
	}
	if (gst_caps_is_empty(caps)) {
		return -1;
	}
	for (i = 0; i < gst_caps_get_size(caps); i++) {
		GstStructure *structure = gst_caps_get_structure(caps, i);
		const gchar* sVal = gst_structure_get_string(structure, "media");
		if (strstr(sVal, "audio") != NULL) {
			puts("audio--------------type");
			result = 1;
			break;
		}
		else if (strstr(sVal, "video") != NULL) {
			puts("video--------------type");
			result = 0;
			break;
		}

	}

#ifndef GST_V16

	for (i = 0; i < gst_caps_get_size(caps); i++) {
		GstStructure *structure = gst_caps_get_structure(caps, i);
		//char buff[128] = {0};
		//bool ret = gst_structure_get(structure, "media", G_TYPE_STRING, buff, NULL);
		gchar* line = gst_structure_serialize(structure, GST_SERIALIZE_FLAG_BACKWARD_COMPAT);
		//g_print("--->>>%s %s\n",  gst_structure_get_name(structure), line);

		if (strstr(line, "audio") != NULL) {
			puts("audio--------------type");
			result = 1;
		}
		else if (strstr(line, "video") != NULL) {
			puts("video--------------type");
			result = 0;
		}

		//gst_structure_foreach(structure, print_field, NULL);
		break;
	}
#endif
	gst_caps_unref(caps);

	return result;
}

void GstUtil::print_caps(const GstCaps * caps, const gchar * pfx) {
	guint i;

	g_return_if_fail(caps != NULL);

	if (gst_caps_is_any(caps)) {
		g_print("%sANY\n", pfx);
		return;
	}
	if (gst_caps_is_empty(caps)) {
		g_print("%sEMPTY\n", pfx);
		return;
	}

	for (i = 0; i < gst_caps_get_size(caps); i++) {
		GstStructure *structure = gst_caps_get_structure(caps, i);

		g_print("%s :name=%s\n\n", pfx, gst_structure_get_name(structure));
		g_print("%s :media---=%s\n", pfx, gst_structure_get_string(structure,"media"));
		
		gst_structure_foreach(structure, print_field, (gpointer)pfx);
	}
}

/* Prints information about a Pad Template, including its Capabilities */
void GstUtil::print_pad_templates_information(GstElementFactory * factory) {
	const GList *pads;
	GstStaticPadTemplate *padtemplate;

	g_print("Pad Templates for %s:\n", gst_element_factory_get_longname(factory));
	return;
	if (!gst_element_factory_get_num_pad_templates(factory)) {
		g_print("  none\n");
		return;
	}

	pads = gst_element_factory_get_static_pad_templates(factory);
	while (pads) {
		padtemplate = (GstStaticPadTemplate *)pads->data;
		pads = g_list_next(pads);

		if (padtemplate->direction == GST_PAD_SRC)
			g_print("  SRC template: '%s'\n", padtemplate->name_template);
		else if (padtemplate->direction == GST_PAD_SINK)
			g_print("  SINK template: '%s'\n", padtemplate->name_template);
		else
			g_print("  UNKNOWN!!! template: '%s'\n", padtemplate->name_template);

		if (padtemplate->presence == GST_PAD_ALWAYS)
			g_print("    Availability: Always\n");
		else if (padtemplate->presence == GST_PAD_SOMETIMES)
			g_print("    Availability: Sometimes\n");
		else if (padtemplate->presence == GST_PAD_REQUEST) {
			g_print("    Availability: On request\n");
		}
		else
			g_print("    Availability: UNKNOWN!!!\n");

		if (padtemplate->static_caps.string) {
			GstCaps *caps;
			g_print("    Capabilities:\n");
			caps = gst_static_caps_get(&padtemplate->static_caps);
			print_caps(caps, "      ");
			gst_caps_unref(caps);

		}

		g_print("\n");
	}
}

/* Shows the CURRENT capabilities of the requested pad in the given element */
void GstUtil::print_pad_capabilities(GstElement *element, gchar *pad_name) {
	GstPad *pad = NULL;
	GstCaps *caps = NULL;

	/* Retrieve pad */
	pad = gst_element_get_static_pad(element, pad_name);
	if (!pad) {
		g_printerr("Could not retrieve pad '%s'\n", pad_name);
		return;
	}

	/* Retrieve negotiated caps (or acceptable caps if negotiation is not finished yet) */
	caps = gst_pad_get_current_caps(pad);
	if (!caps)
		caps = gst_pad_query_caps(pad, NULL);

	/* Print and free */
	g_print("Caps for the %s pad:\n", pad_name);
	print_caps(caps, "      ");
	gst_caps_unref(caps);
	gst_object_unref(pad);
}


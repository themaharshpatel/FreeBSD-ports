--- wireless/wireless-applet.c.orig	Tue Apr 13 06:39:46 2004
+++ wireless/wireless-applet.c	Thu May 13 16:03:29 2004
@@ -30,6 +30,15 @@
 #include <math.h>
 #include <dirent.h>
 
+#ifdef __FreeBSD__
+#include <sys/socket.h>
+#include <sys/ioctl.h>
+#include <net/if.h>
+#include <net/if_var.h>
+#include <dev/an/if_aironet_ieee.h>
+#include <dev/wi/if_wavelan_ieee.h>
+#endif
+
 #include <gnome.h>
 #include <panel-applet.h>
 #include <panel-applet-gconf.h>
@@ -37,7 +46,11 @@
 
 #include <egg-screen-help.h>
 
+#ifdef __FreeBSD__
+#define CFG_DEVICE "an0"
+#else
 #define CFG_DEVICE "eth0"
+#endif
 #define CFG_UPDATE_INTERVAL 2
 
 typedef enum {
@@ -94,6 +107,12 @@
 		WirelessApplet *applet);
 static void wireless_applet_about_cb (BonoboUIComponent *uic,
 		WirelessApplet *applet);
+#ifdef __FreeBSD__
+static int an_getval(WirelessApplet *applet, char *device, struct an_req *areq);
+static void get_an_data(WirelessApplet *applet, char *device, long int *level);
+static int wi_getval(WirelessApplet *applet, char *device, struct wi_req *areq);
+static void get_wi_data(WirelessApplet *applet, char *device, long int *level);
+#endif
 static void prefs_response_cb (GtkDialog *dialog, gint response, gpointer data);
 
 static const BonoboUIVerb wireless_menu_verbs [] = {
@@ -157,9 +176,11 @@
 	g_free (tmp);
 
 	/* Update the image */
+#ifndef __FreeBSD__
 	percent = CLAMP (percent, -1, 100);
+#endif
 
-	if (percent < 0)
+	if (percent < 0 || percent > 100)
 		state = PIX_BROKEN;
 	else if (percent == 0)
 		state = PIX_NO_LINK;
@@ -190,6 +211,7 @@
 	int percent;
 
 	/* Calculate the percentage based on the link quality */
+#ifndef __FreeBSD__
 	if (level < 0) {
 		percent = -1;
 	} else {
@@ -200,6 +222,9 @@
 			percent = CLAMP (percent, 0, 100);
 		}
 	}
+#else
+	percent = (int)level;
+#endif
 
 	wireless_applet_draw (applet, percent);
 }
@@ -242,14 +267,145 @@
 	}
 }
 
+#ifdef __FreeBSD__
+static int
+wi_getval(WirelessApplet *applet, char *device, struct wi_req *wreq)
+{
+	struct ifreq	ifr;
+	int		s;
+ 
+	bzero((char *)&ifr, sizeof(ifr));
+ 
+	strlcpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
+	ifr.ifr_data = (caddr_t)wreq;
+ 
+	s = socket(AF_INET, SOCK_DGRAM, 0);
+ 
+	if (s == -1)
+	{
+		gtk_tooltips_set_tip (applet->tips,
+				GTK_WIDGET (applet),
+				"Socket Error",
+				NULL);
+		return 0;
+	}
+ 
+	if (ioctl(s, SIOCGWAVELAN, &ifr) == -1)
+	{
+		gtk_tooltips_set_tip (applet->tips,
+				GTK_WIDGET (applet),
+				"ioctl Error",
+				NULL);
+		return 0;
+	}
+ 
+	close(s);
+                                                                 
+	return 1;
+}
+
+static void
+get_wi_data (WirelessApplet *applet, char *device, long int *level)
+{
+    	struct wi_req wreq;
+	long int signal_strength;
+
+	bzero((char *)&wreq, sizeof(wreq));
+
+	wreq.wi_len  = WI_MAX_DATALEN;
+	wreq.wi_type = WI_RID_COMMS_QUALITY;
+
+	(void)wi_getval(applet, device, &wreq);
+
+	signal_strength = (long int) (wreq.wi_val[1]);
+
+	memcpy(level, &signal_strength, sizeof(level));
+
+	return;
+}
+
+static int
+an_getval(WirelessApplet *applet, char *device, struct an_req *areq)
+{
+	struct ifreq  ifr;
+	int s;
+
+	bzero((char *)&ifr, sizeof(ifr));
+
+	strlcpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
+	ifr.ifr_data = (caddr_t)areq;
+
+	s = socket(AF_INET, SOCK_DGRAM, 0);
+
+	if (s == -1) {
+		gtk_tooltips_set_tip (applet->tips,
+				GTK_WIDGET (applet),
+				"Socket Error",
+				NULL);
+		return 0;
+	}
+
+	if (ioctl(s, SIOCGAIRONET, &ifr) == -1) {
+		gtk_tooltips_set_tip (applet->tips,
+				GTK_WIDGET (applet),
+				"ioctl Error",
+				NULL);
+		return 0;
+	}
+
+	close(s);
+	return 1;
+}
+
+static void
+get_an_data (WirelessApplet *applet, char *device, long int *level)
+{
+    	struct an_req areq;
+	struct an_ltv_status *sts;
+#ifdef AN_RID_RSSI_MAP
+	struct an_ltv_rssi_map an_rssimap;
+#endif
+	long int signal_strength;
+	int rssimap_valid = 0;
+
+#ifdef AN_RID_RSSI_MAP
+	an_rssimap.an_len = sizeof(an_rssimap);
+	an_rssimap.an_type = AN_RID_RSSI_MAP;
+	rssimap_valid = an_getval(applet, device, (struct an_req*)&an_rssimap);
+#endif
+
+	areq.an_len = sizeof(areq);
+	areq.an_type = AN_RID_STATUS;
+
+	(void)an_getval(applet, device, &areq);
+
+	sts = (struct an_ltv_status *)&areq;
+#ifdef AN_RID_RSSI_MAP
+	if (rssimap_valid)
+	    signal_strength = (long int)(an_rssimap.an_entries[
+		    sts->an_normalized_strength].an_rss_pct);
+	else
+	    signal_strength = (long int)(sts->an_normalized_strength);
+#else
+	signal_strength = (long int)(sts->an_normalized_rssi);
+#endif
+	memcpy(level, &signal_strength, sizeof(level));
+}
+#endif
+
 /* check stats, modify the state attribute */
 static void
 wireless_applet_read_device_state (WirelessApplet *applet)
 {
-	long int level, noise;
-	double link;
+	long int level;
 	char device[256];
+#ifdef __FreeBSD__
+	struct if_nameindex *ifstart, *ifs;
+#else
+	long int noise;
+	double link;
 	char line[256];
+#endif
 	gboolean found = FALSE;
 
 	/* resest list of available wireless devices */
@@ -257,8 +413,35 @@
 	g_list_free (applet->devices);
 	applet->devices = NULL;
 
+#ifdef __FreeBSD__
+	ifs = ifstart = if_nameindex ();
+#endif
+
 	/* Here we begin to suck... */
 	do {
+#ifdef __FreeBSD__
+		if (ifs == NULL || ifs->if_name == NULL) {
+				break;
+		}
+		strlcpy (device, ifs->if_name, 6);
+		if (g_strncasecmp (device, "an", 2)==0) {
+				applet->devices = g_list_prepend (applet->devices, g_strdup (device));
+				if (g_strcasecmp (applet->device, device)==0) {
+						get_an_data (applet, device, &level);
+						wireless_applet_update_state (applet, device, 0, level, 0);
+						found = TRUE;
+				}
+		}
+		else if (g_strncasecmp (device, "wi", 2)==0 || g_strncasecmp (device, "ath", 3)==0) {
+				applet->devices = g_list_prepend (applet->devices, g_strdup (device));
+				if (g_strcasecmp (applet->device, device)==0) {
+						get_wi_data (applet, device, &level);
+						wireless_applet_update_state (applet, device, 0, level, 0);
+						found = TRUE;
+				}
+		}
+		ifs++;
+#else
 		char *ptr;
 
 		fgets (line, 256, applet->file);
@@ -292,6 +475,7 @@
 				found = TRUE;
 			}
 		}
+#endif
 	} while (1);
 
 	if (g_list_length (applet->devices)==1) {
@@ -303,17 +487,23 @@
 	}
 
 	/* rewind the /proc/net/wireless file */
+#ifdef __FreeBSD__
+	if_freenameindex(ifstart);
+#else
 	rewind (applet->file);
+#endif
 }
 
 static int
 wireless_applet_timeout_handler (WirelessApplet *applet)
 {
+#ifndef __FreeBSD__
 	if (applet->file == NULL) {
 		wireless_applet_update_state (applet,
 				applet->device, -1, -1, -1);
 		return FALSE;
 	}
+#endif
 
 	wireless_applet_read_device_state (applet);
 
@@ -360,6 +550,7 @@
 static void
 start_file_read (WirelessApplet *applet)
 {
+#ifndef __FreeBSD__
 	applet->file = fopen ("/proc/net/wireless", "rt");
 	if (applet->file == NULL) {
 		gtk_tooltips_set_tip (applet->tips,
@@ -368,6 +559,7 @@
 				NULL);
 		show_error_dialog (_("There doesn't seem to be any wireless devices configured on your system.\nPlease verify your configuration if you think this is incorrect."));
 	}
+#endif
 }
 
 static void
@@ -670,8 +862,10 @@
 		applet->prefs = NULL;
 	}
 
+#ifndef __FreeBSD__
 	if (applet->file)
 		fclose (applet->file);
+#endif
 	if (applet->tips)
 		g_object_unref (applet->tips);
 }

/*
 * Custom OID/ioctl definitions for
 * Broadcom 802.11abg Networking Device Driver
 *
 * Definitions subject to change without notice.
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: wlioctl.h 433418 2013-10-31 20:16:40Z $
 */

#ifndef _wlioctl_h_
#define	_wlioctl_h_

#include <typedefs.h>
#include <proto/ethernet.h>
#include <proto/bcmip.h>
#include <proto/bcmeth.h>
#include <proto/bcmip.h>
#include <proto/bcmevent.h>
#include <proto/802.11.h>
#include <bcmwifi_channels.h>
#include <bcmwifi_rates.h>
#include <devctrl_if/wlioctl_defs.h>

#ifndef LINUX_POSTMOGRIFY_REMOVAL
#include <bcm_mpool_pub.h>
#include <bcmcdc.h>
#endif 



#ifndef LINUX_POSTMOGRIFY_REMOVAL

#ifndef INTF_NAME_SIZ
#define INTF_NAME_SIZ	16
#endif


typedef struct remote_ioctl {
	cdc_ioctl_t	msg;
	uint32		data_len;
	char           intf_name[INTF_NAME_SIZ];
} rem_ioctl_t;
#define REMOTE_SIZE	sizeof(rem_ioctl_t)

typedef struct {
	uint32 num;
	chanspec_t list[1];
} chanspec_list_t;


typedef struct {
	bool		assoc_approved;		
	uint16		reject_reason;		
	struct		ether_addr   da;
#if 0 && (NDISVER >= 0x0620)
	LARGE_INTEGER	sys_time;		
#else
	int64		sys_time;		
#endif
} assoc_decision_t;

#define ACTION_FRAME_SIZE 1800

typedef struct wl_action_frame {
	struct ether_addr 	da;
	uint16 			len;
	uint32 			packetId;
	uint8			data[ACTION_FRAME_SIZE];
} wl_action_frame_t;

#define WL_WIFI_ACTION_FRAME_SIZE sizeof(struct wl_action_frame)

typedef struct ssid_info
{
	uint8		ssid_len;	
	uint8		ssid[32];	
} ssid_info_t;

typedef struct wl_af_params {
	uint32 			channel;
	int32 			dwell_time;
	struct ether_addr 	BSSID;
	wl_action_frame_t	action_frame;
} wl_af_params_t;

#define WL_WIFI_AF_PARAMS_SIZE sizeof(struct wl_af_params)

#define MFP_TEST_FLAG_NORMAL	0
#define MFP_TEST_FLAG_ANY_KEY	1
typedef struct wl_sa_query {
	uint32			flag;
	uint8 			action;
	uint16 			id;
	struct ether_addr 	da;
} wl_sa_query_t;

#endif 


#define BWL_DEFAULT_PACKING
#include <packed_section_start.h>



#ifndef LINUX_POSTMOGRIFY_REMOVAL


#define	LEGACY_WL_BSS_INFO_VERSION	107	

typedef struct wl_bss_info_107 {
	uint32		version;		
	uint32		length;			
	struct ether_addr BSSID;
	uint16		beacon_period;		
	uint16		capability;		
	uint8		SSID_len;
	uint8		SSID[32];
	struct {
		uint	count;			
		uint8	rates[16];		
	} rateset;				
	uint8		channel;		
	uint16		atim_window;		
	uint8		dtim_period;		
	int16		RSSI;			
	int8		phy_noise;		
	uint32		ie_length;		
	
} wl_bss_info_107_t;



#define	LEGACY2_WL_BSS_INFO_VERSION	108		


typedef struct wl_bss_info_108 {
	uint32		version;		
	uint32		length;			
	struct ether_addr BSSID;
	uint16		beacon_period;		
	uint16		capability;		
	uint8		SSID_len;
	uint8		SSID[32];
	struct {
		uint	count;			
		uint8	rates[16];		
	} rateset;				
	chanspec_t	chanspec;		
	uint16		atim_window;		
	uint8		dtim_period;		
	int16		RSSI;			
	int8		phy_noise;		

	uint8		n_cap;			
	uint32		nbss_cap;		
	uint8		ctl_ch;			
	uint32		reserved32[1];		
	uint8		flags;			
	uint8		reserved[3];		
	uint8		basic_mcs[MCSSET_LEN];	

	uint16		ie_offset;		
	uint32		ie_length;		
	
	
} wl_bss_info_108_t;

#endif 

#define	WL_BSS_INFO_VERSION	109		


typedef struct wl_bss_info {
	uint32		version;		
	uint32		length;			
	struct ether_addr BSSID;
	uint16		beacon_period;		
	uint16		capability;		
	uint8		SSID_len;
	uint8		SSID[32];
	struct {
		uint	count;			
		uint8	rates[16];		
	} rateset;				
	chanspec_t	chanspec;		
	uint16		atim_window;		
	uint8		dtim_period;		
	int16		RSSI;			
	int8		phy_noise;		

	uint8		n_cap;			
	uint32		nbss_cap;		
	uint8		ctl_ch;			
	uint8		padding1[3];		
	uint16		vht_rxmcsmap;	
	uint16		vht_txmcsmap;	
	uint8		flags;			
	uint8		vht_cap;		
	uint8		reserved[2];		
	uint8		basic_mcs[MCSSET_LEN];	

	uint16		ie_offset;		
	uint32		ie_length;		
	int16		SNR;			
	
	
} wl_bss_info_t;

#ifndef LINUX_POSTMOGRIFY_REMOVAL

typedef struct wl_bsscfg {
	uint32  bsscfg_idx;
	uint32  wsec;
	uint32  WPA_auth;
	uint32  wsec_index;
	uint32  associated;
	uint32  BSS;
	uint32  phytest_on;
	struct ether_addr   prev_BSSID;
	struct ether_addr   BSSID;
	uint32  targetbss_wpa2_flags;
	uint32 assoc_type;
	uint32 assoc_state;
} wl_bsscfg_t;

typedef struct wl_if_add {
	uint32  bsscfg_flags;
	uint32  if_flags;
	uint32  ap;
	struct ether_addr   mac_addr;
} wl_if_add_t;

typedef struct wl_bss_config {
	uint32	atim_window;
	uint32	beacon_period;
	uint32	chanspec;
} wl_bss_config_t;

#define WL_BSS_USER_RADAR_CHAN_SELECT	0x1	

#define DLOAD_HANDLER_VER			1	
#define DLOAD_FLAG_VER_MASK		0xf000	
#define DLOAD_FLAG_VER_SHIFT	12	

#define DL_CRC_NOT_INUSE 			0x0001


enum {
	DL_TYPE_UCODE = 1,
	DL_TYPE_CLM = 2
};


enum {
	UCODE_FW,
	INIT_VALS,
	BS_INIT_VALS
};

struct wl_dload_data {
	uint16 flag;
	uint16 dload_type;
	uint32 len;
	uint32 crc;
	uint8  data[1];
};
typedef struct wl_dload_data wl_dload_data_t;

struct wl_ucode_info {
	uint32 ucode_type;
	uint32 num_chunks;
	uint32 chunk_len;
	uint32 chunk_num;
	uint8  data_chunk[1];
};
typedef struct wl_ucode_info wl_ucode_info_t;

struct wl_clm_dload_info {
	uint32 ds_id;
	uint32 clm_total_len;
	uint32 num_chunks;
	uint32 chunk_len;
	uint32 chunk_offset;
	uint8  data_chunk[1];
};
typedef struct wl_clm_dload_info wl_clm_dload_info_t;

#endif 

typedef struct wlc_ssid {
	uint32		SSID_len;
	uchar		SSID[DOT11_MAX_SSID_LEN];
} wlc_ssid_t;

#ifndef LINUX_POSTMOGRIFY_REMOVAL

#define MAX_PREFERRED_AP_NUM     5
typedef struct wlc_fastssidinfo {
	uint32				SSID_channel[MAX_PREFERRED_AP_NUM];
	wlc_ssid_t		SSID_info[MAX_PREFERRED_AP_NUM];
} wlc_fastssidinfo_t;

typedef BWL_PRE_PACKED_STRUCT struct wnm_url {
	uint8   len;
	uint8   data[1];
} BWL_POST_PACKED_STRUCT wnm_url_t;

typedef struct chan_scandata {
	uint8		txpower;
	uint8		pad;
	chanspec_t	channel;	
	uint32		channel_mintime;
	uint32		channel_maxtime;
} chan_scandata_t;

typedef enum wl_scan_type {
	EXTDSCAN_FOREGROUND_SCAN,
	EXTDSCAN_BACKGROUND_SCAN,
	EXTDSCAN_FORCEDBACKGROUND_SCAN
} wl_scan_type_t;

#define WLC_EXTDSCAN_MAX_SSID		5

typedef struct wl_extdscan_params {
	int8 		nprobes;		
	int8    	split_scan;		
	int8		band;			
	int8		pad;
	wlc_ssid_t 	ssid[WLC_EXTDSCAN_MAX_SSID]; 
	uint32		tx_rate;		
	wl_scan_type_t	scan_type;		
	int32 		channel_num;
	chan_scandata_t channel_list[1];	
} wl_extdscan_params_t;

#define WL_EXTDSCAN_PARAMS_FIXED_SIZE 	(sizeof(wl_extdscan_params_t) - sizeof(chan_scandata_t))

#define WL_SCAN_PARAMS_SSID_MAX 	10

typedef struct wl_scan_params {
	wlc_ssid_t ssid;		
	struct ether_addr bssid;	
	int8 bss_type;			
	uint8 scan_type;		
	int32 nprobes;			
	int32 active_time;		
	int32 passive_time;		
	int32 home_time;		
	int32 channel_num;		
	uint16 channel_list[1];		
} wl_scan_params_t;


#define WL_SCAN_PARAMS_FIXED_SIZE 64

#define ISCAN_REQ_VERSION 1


typedef struct wl_iscan_params {
	uint32 version;
	uint16 action;
	uint16 scan_duration;
	wl_scan_params_t params;
} wl_iscan_params_t;


#define WL_ISCAN_PARAMS_FIXED_SIZE (OFFSETOF(wl_iscan_params_t, params) + sizeof(wlc_ssid_t))
#endif 

typedef struct wl_scan_results {
	uint32 buflen;
	uint32 version;
	uint32 count;
	wl_bss_info_t bss_info[1];
} wl_scan_results_t;

#ifndef LINUX_POSTMOGRIFY_REMOVAL

#define WL_SCAN_RESULTS_FIXED_SIZE (sizeof(wl_scan_results_t) - sizeof(wl_bss_info_t))


#define DNGL_RXCTXT_SIZE	45

#if defined(SIMPLE_ISCAN)

#define WLC_IW_ISCAN_MAXLEN   2048
typedef struct iscan_buf {
	struct iscan_buf * next;
	char   iscan_buf[WLC_IW_ISCAN_MAXLEN];
} iscan_buf_t;
#endif 

#define ESCAN_REQ_VERSION 1

typedef struct wl_escan_params {
	uint32 version;
	uint16 action;
	uint16 sync_id;
	wl_scan_params_t params;
} wl_escan_params_t;

#define WL_ESCAN_PARAMS_FIXED_SIZE (OFFSETOF(wl_escan_params_t, params) + sizeof(wlc_ssid_t))

typedef struct wl_escan_result {
	uint32 buflen;
	uint32 version;
	uint16 sync_id;
	uint16 bss_count;
	wl_bss_info_t bss_info[1];
} wl_escan_result_t;

#define WL_ESCAN_RESULTS_FIXED_SIZE (sizeof(wl_escan_result_t) - sizeof(wl_bss_info_t))


typedef struct wl_iscan_results {
	uint32 status;
	wl_scan_results_t results;
} wl_iscan_results_t;


#define WL_ISCAN_RESULTS_FIXED_SIZE \
	(WL_SCAN_RESULTS_FIXED_SIZE + OFFSETOF(wl_iscan_results_t, results))

#define SCANOL_PARAMS_VERSION	1

typedef struct scanol_params {
	uint32 version;
	uint32 flags;	
	int32 active_time;	
	int32 passive_time;	
	int32 idle_rest_time;	
	int32 idle_rest_time_multiplier;
	int32 active_rest_time;
	int32 active_rest_time_multiplier;
	int32 scan_cycle_idle_rest_time;
	int32 scan_cycle_idle_rest_multiplier;
	int32 scan_cycle_active_rest_time;
	int32 scan_cycle_active_rest_multiplier;
	int32 max_rest_time;
	int32 max_scan_cycles;
	int32 nprobes;		
	int32 scan_start_delay;
	uint32 nchannels;
	uint32 ssid_count;
	wlc_ssid_t ssidlist[1];
} scanol_params_t;

typedef struct wl_probe_params {
	wlc_ssid_t ssid;
	struct ether_addr bssid;
	struct ether_addr mac;
} wl_probe_params_t;
#endif 

#define WL_MAXRATES_IN_SET		16	
typedef struct wl_rateset {
	uint32	count;			
	uint8	rates[WL_MAXRATES_IN_SET];	
} wl_rateset_t;

typedef struct wl_rateset_args {
	uint32	count;			
	uint8	rates[WL_MAXRATES_IN_SET];	
	uint8   mcs[MCSSET_LEN];        
	uint16 vht_mcs[VHT_CAP_MCS_MAP_NSS_MAX]; 
} wl_rateset_args_t;

#define TXBF_RATE_MCS_ALL		4
#define TXBF_RATE_VHT_ALL		4
#define TXBF_RATE_OFDM_ALL		8

typedef struct wl_txbf_rateset {
	uint8	txbf_rate_mcs[TXBF_RATE_MCS_ALL];	
	uint8	txbf_rate_mcs_bcm[TXBF_RATE_MCS_ALL];	
	uint16	txbf_rate_vht[TXBF_RATE_VHT_ALL];	
	uint16	txbf_rate_vht_bcm[TXBF_RATE_VHT_ALL];	
	uint8	txbf_rate_ofdm[TXBF_RATE_OFDM_ALL];	
	uint8	txbf_rate_ofdm_bcm[TXBF_RATE_OFDM_ALL]; 
	uint8	txbf_rate_ofdm_cnt;
	uint8	txbf_rate_ofdm_cnt_bcm;
} wl_txbf_rateset_t;

#define OFDM_RATE_MASK			0x0000007f
typedef uint8 ofdm_rates_t;

typedef struct wl_rates_info {
	wl_rateset_t rs_tgt;
	uint32 phy_type;
	int32 bandtype;
	uint8 cck_only;
	uint8 rate_mask;
	uint8 mcsallow;
	uint8 bw;
	uint8 txstreams;
} wl_rates_info_t;


typedef struct wl_uint32_list {
	
	uint32 count;
	
	uint32 element[1];
} wl_uint32_list_t;


typedef struct wl_assoc_params {
	struct ether_addr bssid;	
	uint16 bssid_cnt;		
	int32 chanspec_num;		
	chanspec_t chanspec_list[1];	
} wl_assoc_params_t;

#define WL_ASSOC_PARAMS_FIXED_SIZE 	OFFSETOF(wl_assoc_params_t, chanspec_list)


typedef wl_assoc_params_t wl_reassoc_params_t;
#define WL_REASSOC_PARAMS_FIXED_SIZE	WL_ASSOC_PARAMS_FIXED_SIZE


typedef wl_assoc_params_t wl_join_assoc_params_t;
#define WL_JOIN_ASSOC_PARAMS_FIXED_SIZE	WL_ASSOC_PARAMS_FIXED_SIZE


typedef struct wl_join_params {
	wlc_ssid_t ssid;
	wl_assoc_params_t params;	
} wl_join_params_t;

#ifndef  LINUX_POSTMOGRIFY_REMOVAL
#define WL_JOIN_PARAMS_FIXED_SIZE 	(OFFSETOF(wl_join_params_t, params) + \
					 WL_ASSOC_PARAMS_FIXED_SIZE)

typedef struct wl_join_scan_params {
	uint8 scan_type;		
	int32 nprobes;			
	int32 active_time;		
	int32 passive_time;		
	int32 home_time;		
} wl_join_scan_params_t;


typedef struct wl_extjoin_params {
	wlc_ssid_t ssid;		
	wl_join_scan_params_t scan;
	wl_join_assoc_params_t assoc;	
} wl_extjoin_params_t;
#define WL_EXTJOIN_PARAMS_FIXED_SIZE 	(OFFSETOF(wl_extjoin_params_t, assoc) + \
					 WL_JOIN_ASSOC_PARAMS_FIXED_SIZE)

#define ANT_SELCFG_MAX		4	
#define MAX_STREAMS_SUPPORTED	4	
typedef struct {
	uint8 ant_config[ANT_SELCFG_MAX];	
	uint8 num_antcfg;	
} wlc_antselcfg_t;

typedef struct {
	uint32 duration;	
	uint32 congest_ibss;	
				
	uint32 congest_obss;	
	uint32 interference;	
	uint32 timestamp;	
} cca_congest_t;

typedef struct {
	chanspec_t chanspec;	
	uint8 num_secs;		
	cca_congest_t  secs[1];	
} cca_congest_channel_req_t;


enum interference_source {
	ITFR_NONE = 0,		
	ITFR_PHONE,		
	ITFR_VIDEO_CAMERA,	
	ITFR_MICROWAVE_OVEN,	
	ITFR_BABY_MONITOR,	
	ITFR_BLUETOOTH,		
	ITFR_VIDEO_CAMERA_OR_BABY_MONITOR,	
	ITFR_BLUETOOTH_OR_BABY_MONITOR,	
	ITFR_VIDEO_CAMERA_OR_PHONE,	
	ITFR_UNIDENTIFIED	
};


typedef struct {
	uint32 flags;	
	uint32 source;	
	uint32 timestamp;	
} interference_source_rep_t;
#endif 

#define WLC_CNTRY_BUF_SZ	4		

#ifndef LINUX_POSTMOGRIFY_REMOVAL

typedef struct wl_country {
	char country_abbrev[WLC_CNTRY_BUF_SZ];	
	int32 rev;				
	char ccode[WLC_CNTRY_BUF_SZ];		
} wl_country_t;

typedef struct wl_channels_in_country {
	uint32 buflen;
	uint32 band;
	char country_abbrev[WLC_CNTRY_BUF_SZ];
	uint32 count;
	uint32 channel[1];
} wl_channels_in_country_t;

typedef struct wl_country_list {
	uint32 buflen;
	uint32 band_set;
	uint32 band;
	uint32 count;
	char country_abbrev[1];
} wl_country_list_t;

typedef struct wl_rm_req_elt {
	int8	type;
	int8	flags;
	chanspec_t	chanspec;
	uint32	token;		
	uint32	tsf_h;		
	uint32	tsf_l;		
	uint32	dur;		
} wl_rm_req_elt_t;

typedef struct wl_rm_req {
	uint32	token;		
	uint32	count;		
	void	*cb;		
	void	*cb_arg;	
	wl_rm_req_elt_t	req[1];	
} wl_rm_req_t;
#define WL_RM_REQ_FIXED_LEN	OFFSETOF(wl_rm_req_t, req)

typedef struct wl_rm_rep_elt {
	int8	type;
	int8	flags;
	chanspec_t	chanspec;
	uint32	token;		
	uint32	tsf_h;		
	uint32	tsf_l;		
	uint32	dur;		
	uint32	len;		
	uint8	data[1];	
} wl_rm_rep_elt_t;
#define WL_RM_REP_ELT_FIXED_LEN	24	

#define WL_RPI_REP_BIN_NUM 8
typedef struct wl_rm_rpi_rep {
	uint8	rpi[WL_RPI_REP_BIN_NUM];
	int8	rpi_max[WL_RPI_REP_BIN_NUM];
} wl_rm_rpi_rep_t;

typedef struct wl_rm_rep {
	uint32	token;		
	uint32	len;		
	wl_rm_rep_elt_t	rep[1];	
} wl_rm_rep_t;
#define WL_RM_REP_FIXED_LEN	8


typedef enum sup_auth_status {
	
	WLC_SUP_DISCONNECTED = 0,
	WLC_SUP_CONNECTING,
	WLC_SUP_IDREQUIRED,
	WLC_SUP_AUTHENTICATING,
	WLC_SUP_AUTHENTICATED,
	WLC_SUP_KEYXCHANGE,
	WLC_SUP_KEYED,
	WLC_SUP_TIMEOUT,
	WLC_SUP_LAST_BASIC_STATE,

	
	
	WLC_SUP_KEYXCHANGE_WAIT_M1 = WLC_SUP_AUTHENTICATED,
	
	WLC_SUP_KEYXCHANGE_PREP_M2 = WLC_SUP_KEYXCHANGE,
	
	WLC_SUP_KEYXCHANGE_WAIT_M3 = WLC_SUP_LAST_BASIC_STATE,
	WLC_SUP_KEYXCHANGE_PREP_M4,	
	WLC_SUP_KEYXCHANGE_WAIT_G1,	
	WLC_SUP_KEYXCHANGE_PREP_G2	
} sup_auth_status_t;
#endif 

typedef struct wl_wsec_key {
	uint32		index;		
	uint32		len;		
	uint8		data[DOT11_MAX_KEY_SIZE];	
	uint32		pad_1[18];
	uint32		algo;		
	uint32		flags;		
	uint32		pad_2[2];
	int		pad_3;
	int		iv_initialized;	
	int		pad_4;
	
	struct {
		uint32	hi;		
		uint16	lo;		
	} rxiv;
	uint32		pad_5[2];
	struct ether_addr ea;		
} wl_wsec_key_t;

#define WSEC_MIN_PSK_LEN	8
#define WSEC_MAX_PSK_LEN	64


#define WSEC_PASSPHRASE		(1<<0)


typedef struct {
	ushort	key_len;		
	ushort	flags;			
	uint8	key[WSEC_MAX_PSK_LEN];	
} wsec_pmk_t;

typedef struct _pmkid {
	struct ether_addr	BSSID;
	uint8			PMKID[WPA2_PMKID_LEN];
} pmkid_t;

typedef struct _pmkid_list {
	uint32	npmkid;
	pmkid_t	pmkid[1];
} pmkid_list_t;

typedef struct _pmkid_cand {
	struct ether_addr	BSSID;
	uint8			preauth;
} pmkid_cand_t;

typedef struct _pmkid_cand_list {
	uint32	npmkid_cand;
	pmkid_cand_t	pmkid_cand[1];
} pmkid_cand_list_t;

#define WL_STA_ANT_MAX		4	

#ifndef LINUX_POSTMOGRIFY_REMOVAL
typedef struct wl_assoc_info {
	uint32		req_len;
	uint32		resp_len;
	uint32		flags;
	struct dot11_assoc_req req;
	struct ether_addr reassoc_bssid; 
	struct dot11_assoc_resp resp;
} wl_assoc_info_t;

typedef struct wl_led_info {
	uint32      index;      
	uint32      behavior;
	uint8       activehi;
} wl_led_info_t;



typedef struct {
	uint	byteoff;	
	uint	nbytes;		
	uint16	buf[1];
} srom_rw_t;


typedef struct {
	uint32	source;		
	uint32	byteoff;	
	uint32	nbytes;		
	
} cis_rw_t;


typedef struct {
	uint32	byteoff;	
	uint32	val;		
	uint32	size;		
	uint	band;		
} rw_reg_t;



typedef struct {
	uint16	auto_ctrl;	
	uint16	bb;		
	uint16	radio;		
	uint16	txctl1;		
} atten_t;


struct wme_tx_params_s {
	uint8  short_retry;
	uint8  short_fallback;
	uint8  long_retry;
	uint8  long_fallback;
	uint16 max_rate;  
};

typedef struct wme_tx_params_s wme_tx_params_t;

#define WL_WME_TX_PARAMS_IO_BYTES (sizeof(wme_tx_params_t) * AC_COUNT)

typedef struct wl_plc_nodelist {
	uint32 count;			
	struct _node {
		struct ether_addr ea;	
		uint32 node_type;	
		uint32 cost;		
	} node[1];
} wl_plc_nodelist_t;

typedef struct wl_plc_params {
	uint32	cmd;			
	uint8	plc_failover;		
	struct	ether_addr node_ea;	
	uint32	cost;			
} wl_plc_params_t;


typedef struct {
	int32 ac;
	uint8 val;
	struct ether_addr ea;
} link_val_t;



typedef struct {
	uint16			ver;		
	uint16			len;		
	uint16			cap;		
	uint32			flags;		
	uint32			idle;		
	struct ether_addr	ea;		
	wl_rateset_t		rateset;	
	uint32			in;		
	uint32			listen_interval_inms; 
	uint32			tx_pkts;	
	uint32			tx_failures;	
	uint32			rx_ucast_pkts;	
	uint32			rx_mcast_pkts;	
	uint32			tx_rate;	
	uint32			rx_rate;	
	uint32			rx_decrypt_succeeds;	
	uint32			rx_decrypt_failures;	
	uint32			tx_tot_pkts;	
	uint32			rx_tot_pkts;	
	uint32			tx_mcast_pkts;	
	uint64			tx_tot_bytes;	
	uint64			rx_tot_bytes;	
	uint64			tx_ucast_bytes;	
	uint64			tx_mcast_bytes;	
	uint64			rx_ucast_bytes;	
	uint64			rx_mcast_bytes;	
	int8			rssi[WL_STA_ANT_MAX]; 
	int8			nf[WL_STA_ANT_MAX];	
	uint16			aid;		
	uint16			ht_capabilities;	
	uint16			vht_flags;		
	uint32			tx_pkts_retried; 
	uint32			tx_pkts_retry_exhausted; 
	int8			rx_lastpkt_rssi[WL_STA_ANT_MAX]; 
} sta_info_t;

#define WL_OLD_STAINFO_SIZE	OFFSETOF(sta_info_t, tx_tot_pkts)

#define WL_STA_VER		4

#endif 

#define	WLC_NUMRATES	16	

typedef struct wlc_rateset {
	uint32	count;			
	uint8	rates[WLC_NUMRATES];	
	uint8	htphy_membership;	
	uint8	mcs[MCSSET_LEN];	
	uint16  vht_mcsmap;		
} wlc_rateset_t;


typedef struct {
	uint32	val;
	struct ether_addr ea;
} scb_val_t;


typedef struct {
	uint32 code;
	scb_val_t ioctl_args;
} authops_t;


typedef struct channel_info {
	int hw_channel;
	int target_channel;
	int scan_channel;
} channel_info_t;


struct maclist {
	uint count;			
	struct ether_addr ea[1];	
};

#ifndef LINUX_POSTMOGRIFY_REMOVAL

typedef struct get_pktcnt {
	uint rx_good_pkt;
	uint rx_bad_pkt;
	uint tx_good_pkt;
	uint tx_bad_pkt;
	uint rx_ocast_good_pkt; 
} get_pktcnt_t;


#define LQ_IDX_MIN              0
#define LQ_IDX_MAX              1
#define LQ_IDX_AVG              2
#define LQ_IDX_SUM              2
#define LQ_IDX_LAST             3
#define LQ_STOP_MONITOR         0
#define LQ_START_MONITOR        1


typedef struct {
	int rssi[LQ_IDX_LAST];  
	int snr[LQ_IDX_LAST];   
	int isvalid;            
} wl_lq_t; 

typedef enum wl_wakeup_reason_type {
	LCD_ON = 1,
	LCD_OFF,
	DRC1_WAKE,
	DRC2_WAKE,
	REASON_LAST
} wl_wr_type_t;

typedef struct {

	uint32	id;


	uint8	reason;
} wl_wr_t;


typedef struct {
	struct	ether_addr ea;	
	uint8	ac_cat;	
	uint8	num_pkts;	
} wl_mac_ratehisto_cmd_t;	


typedef struct {
	uint32	rate[DOT11_RATE_MAX + 1];	
	uint32	mcs[WL_RATESET_SZ_HT_MCS * WL_TX_CHAINS_MAX];	
	uint32	vht[WL_RATESET_SZ_VHT_MCS][WL_TX_CHAINS_MAX];	
	uint32	tsf_timer[2][2];	
} wl_mac_ratehisto_res_t;	

#endif 


typedef struct wl_ioctl {
	uint cmd;	
	void *buf;	
	uint len;	
	uint8 set;		
	uint used;	
	uint needed;	
} wl_ioctl_t;

#ifndef LINUX_POSTMOGRIFY_REMOVAL


typedef struct wlc_rev_info {
	uint		vendorid;	
	uint		deviceid;	
	uint		radiorev;	
	uint		chiprev;	
	uint		corerev;	
	uint		boardid;	
	uint		boardvendor;	
	uint		boardrev;	
	uint		driverrev;	
	uint		ucoderev;	
	uint		bus;		
	uint		chipnum;	
	uint		phytype;	
	uint		phyrev;		
	uint		anarev;		
	uint		chippkg;	
	uint		nvramrev;	
} wlc_rev_info_t;

#define WL_REV_INFO_LEGACY_LENGTH	48

#define WL_BRAND_MAX 10
typedef struct wl_instance_info {
	uint instance;
	char brand[WL_BRAND_MAX];
} wl_instance_info_t;


typedef struct wl_txfifo_sz {
	uint16	magic;
	uint16	fifo;
	uint16	size;
} wl_txfifo_sz_t;



#define WLC_IOV_NAME_LEN 30
typedef struct wlc_iov_trx_s {
	uint8 module;
	uint8 type;
	char name[WLC_IOV_NAME_LEN];
} wlc_iov_trx_t;


#define WLC_IOCTL_VERSION	2
#define WLC_IOCTL_VERSION_LEGACY_IOTYPES	1

#ifdef CONFIG_USBRNDIS_RETAIL

typedef struct {
	char *name;
	void *param;
} ndconfig_item_t;
#endif


#define WL_PHY_PAVARS_LEN	32	

#define WL_PHY_PAVAR_VER	1	
#define WL_PHY_PAVARS2_NUM	3	
typedef struct wl_pavars2 {
	uint16 ver;		
	uint16 len;		
	uint16 inuse;		
	uint16 phy_type;	
	uint16 bandrange;
	uint16 chain;
	uint16 inpa[WL_PHY_PAVARS2_NUM];	
} wl_pavars2_t;

typedef struct wl_po {
	uint16	phy_type;	
	uint16	band;
	uint16	cckpo;
	uint32	ofdmpo;
	uint16	mcspo[8];
} wl_po_t;

#define WL_NUM_RPCALVARS 5	

typedef struct wl_rpcal {
	uint16 value;
	uint16 update;
} wl_rpcal_t;

typedef struct wl_aci_args {
	int enter_aci_thresh; 
	int exit_aci_thresh; 
	int usec_spin; 
	int glitch_delay; 
	uint16 nphy_adcpwr_enter_thresh;	
	uint16 nphy_adcpwr_exit_thresh;	
	uint16 nphy_repeat_ctr;		
	uint16 nphy_num_samples;	
	uint16 nphy_undetect_window_sz;	
	uint16 nphy_b_energy_lo_aci;	
	uint16 nphy_b_energy_md_aci;	
	uint16 nphy_b_energy_hi_aci;	
	uint16 nphy_noise_noassoc_glitch_th_up; 
	uint16 nphy_noise_noassoc_glitch_th_dn;
	uint16 nphy_noise_assoc_glitch_th_up;
	uint16 nphy_noise_assoc_glitch_th_dn;
	uint16 nphy_noise_assoc_aci_glitch_th_up;
	uint16 nphy_noise_assoc_aci_glitch_th_dn;
	uint16 nphy_noise_assoc_enter_th;
	uint16 nphy_noise_noassoc_enter_th;
	uint16 nphy_noise_assoc_rx_glitch_badplcp_enter_th;
	uint16 nphy_noise_noassoc_crsidx_incr;
	uint16 nphy_noise_assoc_crsidx_incr;
	uint16 nphy_noise_crsidx_decr;
} wl_aci_args_t;

#define WL_ACI_ARGS_LEGACY_LENGTH	16	
#define	WL_SAMPLECOLLECT_T_VERSION	2	
typedef struct wl_samplecollect_args {
	
	uint8 coll_us;
	int cores;
	
	uint16 version;     
	uint16 length;      
	int8 trigger;
	uint16 timeout;
	uint16 mode;
	uint32 pre_dur;
	uint32 post_dur;
	uint8 gpio_sel;
	uint8 downsamp;
	uint8 be_deaf;
	uint8 agc;		
	uint8 filter;		
	
	uint8 trigger_state;
	uint8 module_sel1;
	uint8 module_sel2;
	uint16 nsamps;
	int bitStart;
	uint32 gpioCapMask;
} wl_samplecollect_args_t;

#define	WL_SAMPLEDATA_T_VERSION		1	

#define	WL_SAMPLEDATA_T_VERSION_SPEC_AN 2

typedef struct wl_sampledata {
	uint16 version;	
	uint16 size;	
	uint16 tag;	
	uint16 length;	
	uint32 flag;	
} wl_sampledata_t;




enum {
	WL_OTA_TEST_IDLE = 0,	
	WL_OTA_TEST_ACTIVE = 1,	
	WL_OTA_TEST_SUCCESS = 2,	
	WL_OTA_TEST_FAIL = 3	
};

enum {
	WL_OTA_SYNC_IDLE = 0,	
	WL_OTA_SYNC_ACTIVE = 1,	
	WL_OTA_SYNC_FAIL = 2	
};


enum {
	WL_OTA_SKIP_TEST_CAL_FAIL = 1,		
	WL_OTA_SKIP_TEST_SYNCH_FAIL = 2,		
	WL_OTA_SKIP_TEST_FILE_DWNLD_FAIL = 3,	
	WL_OTA_SKIP_TEST_NO_TEST_FOUND = 4,	
	WL_OTA_SKIP_TEST_WL_NOT_UP = 5,		
	WL_OTA_SKIP_TEST_UNKNOWN_CALL		
};


enum {
	WL_OTA_TEST_TX = 0,		
	WL_OTA_TEST_RX = 1,		
};


enum {
	WL_OTA_TEST_BW_20_IN_40MHZ = 0,	
	WL_OTA_TEST_BW_20MHZ = 1,		
	WL_OTA_TEST_BW_40MHZ = 2		
};
typedef struct ota_rate_info {
	uint8 rate_cnt;					
	uint8 rate_val_mbps[WL_OTA_TEST_MAX_NUM_RATE];	
							
							
} ota_rate_info_t;

typedef struct ota_power_info {
	int8 pwr_ctrl_on;	
	int8 start_pwr;		
	int8 delta_pwr;		
	int8 end_pwr;		
} ota_power_info_t;

typedef struct ota_packetengine {
	uint16 delay;           
				
				
	uint16 nframes;         
	uint16 length;          
} ota_packetengine_t;


typedef struct wl_ota_test_args {
	uint8 cur_test;			
	uint8 chan;			
	uint8 bw;			
	uint8 control_band;		
	uint8 stf_mode;			
	ota_rate_info_t rt_info;	
	ota_packetengine_t pkteng;	
	uint8 txant;			
	uint8 rxant;			
	ota_power_info_t pwr_info;	
	uint8 wait_for_sync;		
} wl_ota_test_args_t;

typedef struct wl_ota_test_vector {
	wl_ota_test_args_t test_arg[WL_OTA_TEST_MAX_NUM_SEQ];	
	uint16 test_cnt;					
	uint8 file_dwnld_valid;					
	uint8 sync_timeout;					
	int8 sync_fail_action;					
	struct ether_addr sync_mac;				
	struct ether_addr tx_mac;				
	struct ether_addr rx_mac;				
	int8 loop_test;					
} wl_ota_test_vector_t;



typedef struct wl_ota_test_status {
	int16 cur_test_cnt;		
	int8 skip_test_reason;		
	wl_ota_test_args_t test_arg;	
	uint16 test_cnt;		
	uint8 file_dwnld_valid;		
	uint8 sync_timeout;		
	int8 sync_fail_action;		
	struct ether_addr sync_mac;	
	struct ether_addr tx_mac;	
	struct ether_addr rx_mac;	
	uint8  test_stage;		
	int8 loop_test;		
	uint8 sync_status;		
} wl_ota_test_status_t;




typedef struct {
	int npulses;	
	int ncontig;	
	int min_pw;	
	int max_pw;	
	uint16 thresh0;	
	uint16 thresh1;	
	uint16 blank;	
	uint16 fmdemodcfg;	
	int npulses_lp;  
	int min_pw_lp; 
	int max_pw_lp; 
	int min_fm_lp; 
	int max_span_lp;  
	int min_deltat; 
	int max_deltat; 
	uint16 autocorr;	
	uint16 st_level_time;	
	uint16 t2_min; 
	uint32 version; 
	uint32 fra_pulse_err;	
	int npulses_fra;  
	int npulses_stg2;  
	int npulses_stg3;  
	uint16 percal_mask;	
	int quant;	
	uint32 min_burst_intv_lp;	
	uint32 max_burst_intv_lp;	
	int nskip_rst_lp;	
	int max_pw_tol;	
	uint16 feature_mask; 
} wl_radar_args_t;

#define WL_RADAR_ARGS_VERSION 2

typedef struct {
	uint32 version; 
	uint16 thresh0_20_lo;	
	uint16 thresh1_20_lo;	
	uint16 thresh0_40_lo;	
	uint16 thresh1_40_lo;	
	uint16 thresh0_80_lo;	
	uint16 thresh1_80_lo;	
	uint16 thresh0_20_hi;	
	uint16 thresh1_20_hi;	
	uint16 thresh0_40_hi;	
	uint16 thresh1_40_hi;	
	uint16 thresh0_80_hi;	
	uint16 thresh1_80_hi;	
#ifdef WL11AC160
	uint16 thresh0_160_lo;	
	uint16 thresh1_160_lo;	
	uint16 thresh0_160_hi;	
	uint16 thresh1_160_hi;	
#endif 
} wl_radar_thr_t;

#define WL_RADAR_THR_VERSION	2


typedef struct {
	uint32	version;		
	uint32	count;			
	int8 rssi_ant[WL_RSSI_ANT_MAX];	
} wl_rssi_ant_t;


typedef struct {
	uint state;		
	uint duration;		
	
	chanspec_t chanspec_cleared;
	
	uint16 pad;
} wl_dfs_status_t;


typedef struct {
	bool detected;
	int count;
	bool pretended;
	uint32 radartype;
	uint32 timenow;
	uint32 timefromL;
	int lp_csect_single;
	int detected_pulse_index;
	int nconsecq_pulses;
	chanspec_t ch;
	int pw[10];
	int intv[10];
	int fm[10];
} wl_radar_status_t;

#define NUM_PWRCTRL_RATES 12

typedef struct {
	uint8 txpwr_band_max[NUM_PWRCTRL_RATES];	
	uint8 txpwr_limit[NUM_PWRCTRL_RATES];		
	uint8 txpwr_local_max;				
	uint8 txpwr_local_constraint;			
	uint8 txpwr_chan_reg_max;			
	uint8 txpwr_target[2][NUM_PWRCTRL_RATES];	
	uint8 txpwr_est_Pout[2];			
	uint8 txpwr_opo[NUM_PWRCTRL_RATES];		
	uint8 txpwr_bphy_cck_max[NUM_PWRCTRL_RATES];	
	uint8 txpwr_bphy_ofdm_max;			
	uint8 txpwr_aphy_max[NUM_PWRCTRL_RATES];	
	int8  txpwr_antgain[2];				
	uint8 txpwr_est_Pout_gofdm;			
} tx_power_legacy_t;

#define WL_TX_POWER_RATES_LEGACY    45
#define WL_TX_POWER_MCS20_FIRST         12
#define WL_TX_POWER_MCS20_NUM           16
#define WL_TX_POWER_MCS40_FIRST         28
#define WL_TX_POWER_MCS40_NUM           17

typedef struct {
	uint32 flags;
	chanspec_t chanspec;                 
	chanspec_t local_chanspec;           
	uint8 local_max;                 
	uint8 local_constraint;              
	int8  antgain[2];                
	uint8 rf_cores;                  
	uint8 est_Pout[4];                           
	uint8 est_Pout_cck;                          
	uint8 user_limit[WL_TX_POWER_RATES_LEGACY];  
	uint8 reg_limit[WL_TX_POWER_RATES_LEGACY];   
	uint8 board_limit[WL_TX_POWER_RATES_LEGACY]; 
	uint8 target[WL_TX_POWER_RATES_LEGACY];      
} tx_power_legacy2_t;


#define WL_NUM_RATES_CCK			4 
#define WL_NUM_RATES_OFDM			8 
#define WL_NUM_RATES_MCS_1STREAM	8 
#define WL_NUM_RATES_EXTRA_VHT		2 
#define WL_NUM_RATES_VHT			10
#define WL_NUM_RATES_MCS32			1

#define WLC_NUM_RATES_CCK       WL_NUM_RATES_CCK
#define WLC_NUM_RATES_OFDM      WL_NUM_RATES_OFDM
#define WLC_NUM_RATES_MCS_1_STREAM  WL_NUM_RATES_MCS_1STREAM
#define WLC_NUM_RATES_MCS_2_STREAM  WL_NUM_RATES_MCS_1STREAM
#define WLC_NUM_RATES_MCS32     WL_NUM_RATES_MCS32
#define WL_TX_POWER_CCK_NUM     WL_NUM_RATES_CCK
#define WL_TX_POWER_OFDM_NUM        WL_NUM_RATES_OFDM
#define WL_TX_POWER_MCS_1_STREAM_NUM    WL_NUM_RATES_MCS_1STREAM
#define WL_TX_POWER_MCS_2_STREAM_NUM    WL_NUM_RATES_MCS_1STREAM
#define WL_TX_POWER_MCS_32_NUM      WL_NUM_RATES_MCS32

#define WL_NUM_2x2_ELEMENTS		4
#define WL_NUM_3x3_ELEMENTS		6

typedef struct {
	uint16 ver;				
	uint16 len;				
	uint32 flags;
	chanspec_t chanspec;			
	chanspec_t local_chanspec;		
	uint32     buflen;			
	uint8      pprbuf[1];			
} wl_txppr_t;

#define WL_TXPPR_VERSION	1
#define WL_TXPPR_LENGTH	(sizeof(wl_txppr_t))
#define TX_POWER_T_VERSION	44


typedef struct tx_inst_power {
	uint8 txpwr_est_Pout[2];			
	uint8 txpwr_est_Pout_gofdm;			
} tx_inst_power_t;

#define WL_NUM_TXCHAIN_MAX	4
typedef struct wl_txchain_pwr_offsets {
	int8 offset[WL_NUM_TXCHAIN_MAX];	
} wl_txchain_pwr_offsets_t;

#define WL_NUMCHANNELS		64



struct tsinfo_arg {
	uint8 octets[3];
};
#endif 

#define	NFIFO			6	
#define NREINITREASONCOUNT	8
#define REINITREASONIDX(_x)	(((_x) < NREINITREASONCOUNT) ? (_x) : 0)

#define	WL_CNT_T_VERSION	10	

typedef struct {
	uint16	version;	
	uint16	length;		

	
	uint32	txframe;	
	uint32	txbyte;		
	uint32	txretrans;	
	uint32	txerror;	
	uint32	txctl;		
	uint32	txprshort;	
	uint32	txserr;		
	uint32	txnobuf;	
	uint32	txnoassoc;	
	uint32	txrunt;		
	uint32	txchit;		
	uint32	txcmiss;	

	
	uint32	txuflo;		
	uint32	txphyerr;	
	uint32	txphycrs;

	
	uint32	rxframe;	
	uint32	rxbyte;		
	uint32	rxerror;	
	uint32	rxctl;		
	uint32	rxnobuf;	
	uint32	rxnondata;	
	uint32	rxbadds;	
	uint32	rxbadcm;	
	uint32	rxfragerr;	
	uint32	rxrunt;		
	uint32	rxgiant;	
	uint32	rxnoscb;	
	uint32	rxbadproto;	
	uint32	rxbadsrcmac;	
	uint32	rxbadda;	
	uint32	rxfilter;	

	
	uint32	rxoflo;		
	uint32	rxuflo[NFIFO];	

	uint32	d11cnt_txrts_off;	
	uint32	d11cnt_rxcrc_off;	
	uint32	d11cnt_txnocts_off;	

	
	uint32	dmade;		
	uint32	dmada;		
	uint32	dmape;		
	uint32	reset;		
	uint32	tbtt;		
	uint32	txdmawar;
	uint32	pkt_callback_reg_fail;	

	
	uint32	txallfrm;	
	uint32	txrtsfrm;	
	uint32	txctsfrm;	
	uint32	txackfrm;	
	uint32	txdnlfrm;	
	uint32	txbcnfrm;	
	uint32	txfunfl[6];	
	uint32	rxtoolate;	
	uint32  txfbw;		
	uint32	txtplunfl;	
	uint32	txphyerror;	
	uint32	rxfrmtoolong;	
	uint32	rxfrmtooshrt;	
	uint32	rxinvmachdr;	
	uint32	rxbadfcs;	
	uint32	rxbadplcp;	
	uint32	rxcrsglitch;	
	uint32	rxstrt;		
	uint32	rxdfrmucastmbss; 
	uint32	rxmfrmucastmbss; 
	uint32	rxcfrmucast;	
	uint32	rxrtsucast;	
	uint32	rxctsucast;	
	uint32	rxackucast;	
	uint32	rxdfrmocast;	
	uint32	rxmfrmocast;	
	uint32	rxcfrmocast;	
	uint32	rxrtsocast;	
	uint32	rxctsocast;	
	uint32	rxdfrmmcast;	
	uint32	rxmfrmmcast;	
	uint32	rxcfrmmcast;	
	uint32	rxbeaconmbss;	
	uint32	rxdfrmucastobss; 
	uint32	rxbeaconobss;	
	uint32	rxrsptmout;	
	uint32	bcntxcancl;	
	uint32	rxf0ovfl;	
	uint32	rxf1ovfl;	
	uint32	rxf2ovfl;	
	uint32	txsfovfl;	
	uint32	pmqovfl;	
	uint32	rxcgprqfrm;	
	uint32	rxcgprsqovfl;	
	uint32	txcgprsfail;	
	uint32	txcgprssuc;	
	uint32	prs_timeout;	
	uint32	rxnack;		
	uint32	frmscons;	
	uint32  txnack;		
	uint32	rxback;		
	uint32	txback;		

	
	uint32	txfrag;		
	uint32	txmulti;	
	uint32	txfail;		
	uint32	txretry;	
	uint32	txretrie;	
	uint32	rxdup;		
	uint32	txrts;		
	uint32	txnocts;	
	uint32	txnoack;	
	uint32	rxfrag;		
	uint32	rxmulti;	
	uint32	rxcrc;		
	uint32	txfrmsnt;	
	uint32	rxundec;	

	
	uint32	tkipmicfaill;	
	uint32	tkipcntrmsr;	
	uint32	tkipreplay;	
	uint32	ccmpfmterr;	
	uint32	ccmpreplay;	
	uint32	ccmpundec;	
	uint32	fourwayfail;	
	uint32	wepundec;	
	uint32	wepicverr;	
	uint32	decsuccess;	
	uint32	tkipicverr;	
	uint32	wepexcluded;	

	uint32	txchanrej;	
	uint32	psmwds;		
	uint32	phywatchdog;	

	
	uint32	prq_entries_handled;	
	uint32	prq_undirected_entries;	
	uint32	prq_bad_entries;	
	uint32	atim_suppress_count;	
	uint32	bcn_template_not_ready;	
	uint32	bcn_template_not_ready_done; 
	uint32	late_tbtt_dpc;	

	
	uint32  rx1mbps;	
	uint32  rx2mbps;	
	uint32  rx5mbps5;	
	uint32  rx6mbps;	
	uint32  rx9mbps;	
	uint32  rx11mbps;	
	uint32  rx12mbps;	
	uint32  rx18mbps;	
	uint32  rx24mbps;	
	uint32  rx36mbps;	
	uint32  rx48mbps;	
	uint32  rx54mbps;	
	uint32  rx108mbps;	
	uint32  rx162mbps;	
	uint32  rx216mbps;	
	uint32  rx270mbps;	
	uint32  rx324mbps;	
	uint32  rx378mbps;	
	uint32  rx432mbps;	
	uint32  rx486mbps;	
	uint32  rx540mbps;	

	
	uint32	pktengrxducast; 
	uint32	pktengrxdmcast; 

	uint32	rfdisable;	
	uint32	bphy_rxcrsglitch;	
	uint32  bphy_badplcp;

	uint32	txexptime;	

	uint32	txmpdu_sgi;	
	uint32	rxmpdu_sgi;	
	uint32	txmpdu_stbc;	
	uint32	rxmpdu_stbc;	

	uint32	rxundec_mcst;	

	
	uint32	tkipmicfaill_mcst;	
	uint32	tkipcntrmsr_mcst;	
	uint32	tkipreplay_mcst;	
	uint32	ccmpfmterr_mcst;	
	uint32	ccmpreplay_mcst;	
	uint32	ccmpundec_mcst;	
	uint32	fourwayfail_mcst;	
	uint32	wepundec_mcst;	
	uint32	wepicverr_mcst;	
	uint32	decsuccess_mcst;	
	uint32	tkipicverr_mcst;	
	uint32	wepexcluded_mcst;	

	uint32	dma_hang;	
	uint32	reinit;		

	uint32  pstatxucast;	
	uint32  pstatxnoassoc;	
	uint32  pstarxucast;	
	uint32  pstarxbcmc;	
	uint32  pstatxbcmc;	

	uint32  cso_passthrough; 
	uint32	cso_normal;	
	uint32	chained;	
	uint32	chainedsz1;	
	uint32	unchained;	
	uint32	maxchainsz;	
	uint32	currchainsz;	
	uint32	rxdrop20s;	
	uint32	pciereset;	
	uint32	cfgrestore;	
	uint32	reinitreason[NREINITREASONCOUNT]; 
} wl_cnt_t;

#ifndef LINUX_POSTMOGRIFY_REMOVAL
typedef struct {
	uint16  version;    
	uint16  length;     

	
	uint32  txframe;    
	uint32  txbyte;     
	uint32  txretrans;  
	uint32  txerror;    
	uint32  txctl;      
	uint32  txprshort;  
	uint32  txserr;     
	uint32  txnobuf;    
	uint32  txnoassoc;  
	uint32  txrunt;     
	uint32  txchit;     
	uint32  txcmiss;    

	
	uint32  txuflo;     
	uint32  txphyerr;   
	uint32  txphycrs;

	
	uint32  rxframe;    
	uint32  rxbyte;     
	uint32  rxerror;    
	uint32  rxctl;      
	uint32  rxnobuf;    
	uint32  rxnondata;  
	uint32  rxbadds;    
	uint32  rxbadcm;    
	uint32  rxfragerr;  
	uint32  rxrunt;     
	uint32  rxgiant;    
	uint32  rxnoscb;    
	uint32  rxbadproto; 
	uint32  rxbadsrcmac;    
	uint32  rxbadda;    
	uint32  rxfilter;   

	
	uint32  rxoflo;     
	uint32  rxuflo[NFIFO];  

	uint32  d11cnt_txrts_off;   
	uint32  d11cnt_rxcrc_off;   
	uint32  d11cnt_txnocts_off; 

	
	uint32  dmade;      
	uint32  dmada;      
	uint32  dmape;      
	uint32  reset;      
	uint32  tbtt;       
	uint32  txdmawar;
	uint32  pkt_callback_reg_fail;  

	
	uint32  txallfrm;   
	uint32  txrtsfrm;   
	uint32  txctsfrm;   
	uint32  txackfrm;   
	uint32  txdnlfrm;   
	uint32  txbcnfrm;   
	uint32  txfunfl[6]; 
	uint32	rxtoolate;	
	uint32  txfbw;	    
	uint32  txtplunfl;  
	uint32  txphyerror; 
	uint32  rxfrmtoolong;   
	uint32  rxfrmtooshrt;   
	uint32  rxinvmachdr;    
	uint32  rxbadfcs;   
	uint32  rxbadplcp;  
	uint32  rxcrsglitch;    
	uint32  rxstrt;     
	uint32  rxdfrmucastmbss; 
	uint32  rxmfrmucastmbss; 
	uint32  rxcfrmucast;    
	uint32  rxrtsucast; 
	uint32  rxctsucast; 
	uint32  rxackucast; 
	uint32  rxdfrmocast;    
	uint32  rxmfrmocast;    
	uint32  rxcfrmocast;    
	uint32  rxrtsocast; 
	uint32  rxctsocast; 
	uint32  rxdfrmmcast;    
	uint32  rxmfrmmcast;    
	uint32  rxcfrmmcast;    
	uint32  rxbeaconmbss;   
	uint32  rxdfrmucastobss; 
	uint32  rxbeaconobss;   
	uint32  rxrsptmout; 
	uint32  bcntxcancl; 
	uint32  rxf0ovfl;   
	uint32  rxf1ovfl;   
	uint32  rxf2ovfl;   
	uint32  txsfovfl;   
	uint32  pmqovfl;    
	uint32  rxcgprqfrm; 
	uint32  rxcgprsqovfl;   
	uint32  txcgprsfail;    
	uint32  txcgprssuc; 
	uint32  prs_timeout;    
	uint32  rxnack;
	uint32  frmscons;
	uint32  txnack;		
	uint32	rxback;		
	uint32	txback;		

	
	uint32  txfrag;     
	uint32  txmulti;    
	uint32  txfail;     
	uint32  txretry;    
	uint32  txretrie;   
	uint32  rxdup;      
	uint32  txrts;      
	uint32  txnocts;    
	uint32  txnoack;    
	uint32  rxfrag;     
	uint32  rxmulti;    
	uint32  rxcrc;      
	uint32  txfrmsnt;   
	uint32  rxundec;    

	
	uint32  tkipmicfaill;   
	uint32  tkipcntrmsr;    
	uint32  tkipreplay; 
	uint32  ccmpfmterr; 
	uint32  ccmpreplay; 
	uint32  ccmpundec;  
	uint32  fourwayfail;    
	uint32  wepundec;   
	uint32  wepicverr;  
	uint32  decsuccess; 
	uint32  tkipicverr; 
	uint32  wepexcluded;    

	uint32  rxundec_mcst;   

	
	uint32  tkipmicfaill_mcst;  
	uint32  tkipcntrmsr_mcst;   
	uint32  tkipreplay_mcst;    
	uint32  ccmpfmterr_mcst;    
	uint32  ccmpreplay_mcst;    
	uint32  ccmpundec_mcst; 
	uint32  fourwayfail_mcst;   
	uint32  wepundec_mcst;  
	uint32  wepicverr_mcst; 
	uint32  decsuccess_mcst;    
	uint32  tkipicverr_mcst;    
	uint32  wepexcluded_mcst;   

	uint32  txchanrej;  
	uint32  txexptime;  
	uint32  psmwds;     
	uint32  phywatchdog;    

	
	uint32  prq_entries_handled;    
	uint32  prq_undirected_entries; 
	uint32  prq_bad_entries;    
	uint32  atim_suppress_count;    
	uint32  bcn_template_not_ready; 
	uint32  bcn_template_not_ready_done; 
	uint32  late_tbtt_dpc;  

	
	uint32  rx1mbps;    
	uint32  rx2mbps;    
	uint32  rx5mbps5;   
	uint32  rx6mbps;    
	uint32  rx9mbps;    
	uint32  rx11mbps;   
	uint32  rx12mbps;   
	uint32  rx18mbps;   
	uint32  rx24mbps;   
	uint32  rx36mbps;   
	uint32  rx48mbps;   
	uint32  rx54mbps;   
	uint32  rx108mbps;  
	uint32  rx162mbps;  
	uint32  rx216mbps;  
	uint32  rx270mbps;  
	uint32  rx324mbps;  
	uint32  rx378mbps;  
	uint32  rx432mbps;  
	uint32  rx486mbps;  
	uint32  rx540mbps;  

	
	uint32  pktengrxducast; 
	uint32  pktengrxdmcast; 

	uint32  rfdisable;  
	uint32  bphy_rxcrsglitch;   
	uint32  bphy_badplcp;

	uint32  txmpdu_sgi; 
	uint32  rxmpdu_sgi; 
	uint32  txmpdu_stbc;    
	uint32  rxmpdu_stbc;    

	uint32	rxdrop20s;	

} wl_cnt_ver_six_t;

#define	WL_DELTA_STATS_T_VERSION	2	

typedef struct {
	uint16 version;     
	uint16 length;      

	
	uint32 txframe;     
	uint32 txbyte;      
	uint32 txretrans;   
	uint32 txfail;      

	
	uint32 rxframe;     
	uint32 rxbyte;      

	
	uint32  rx1mbps;	
	uint32  rx2mbps;	
	uint32  rx5mbps5;	
	uint32  rx6mbps;	
	uint32  rx9mbps;	
	uint32  rx11mbps;	
	uint32  rx12mbps;	
	uint32  rx18mbps;	
	uint32  rx24mbps;	
	uint32  rx36mbps;	
	uint32  rx48mbps;	
	uint32  rx54mbps;	
	uint32  rx108mbps;	
	uint32  rx162mbps;	
	uint32  rx216mbps;	
	uint32  rx270mbps;	
	uint32  rx324mbps;	
	uint32  rx378mbps;	
	uint32  rx432mbps;	
	uint32  rx486mbps;	
	uint32  rx540mbps;	

	
	uint32 rxbadplcp;
	uint32 rxcrsglitch;
	uint32 bphy_rxcrsglitch;
	uint32 bphy_badplcp;

} wl_delta_stats_t;
#endif 

typedef struct {
	uint32 packets;
	uint32 bytes;
} wl_traffic_stats_t;

typedef struct {
	uint16	version;	
	uint16	length;		

	wl_traffic_stats_t tx[AC_COUNT];	
	wl_traffic_stats_t tx_failed[AC_COUNT];	
	wl_traffic_stats_t rx[AC_COUNT];	
	wl_traffic_stats_t rx_failed[AC_COUNT];	

	wl_traffic_stats_t forward[AC_COUNT];	

	wl_traffic_stats_t tx_expired[AC_COUNT];	

} wl_wme_cnt_t;

#ifndef LINUX_POSTMOGRIFY_REMOVAL
struct wl_msglevel2 {
	uint32 low;
	uint32 high;
};

typedef struct wl_mkeep_alive_pkt {
	uint16	version; 
	uint16	length; 
	uint32	period_msec;
	uint16	len_bytes;
	uint8	keep_alive_id; 
	uint8	data[1];
} wl_mkeep_alive_pkt_t;

#define WL_MKEEP_ALIVE_VERSION		1
#define WL_MKEEP_ALIVE_FIXED_LEN	OFFSETOF(wl_mkeep_alive_pkt_t, data)
#define WL_MKEEP_ALIVE_PRECISION	500


typedef struct wl_mtcpkeep_alive_conn_pkt {
	struct ether_addr saddr;		
	struct ether_addr daddr;		
	struct ipv4_addr sipaddr;		
	struct ipv4_addr dipaddr;		
	uint16 sport;				
	uint16 dport;				
	uint32 seq;				
	uint32 ack;				
	uint16 tcpwin;				
} wl_mtcpkeep_alive_conn_pkt_t;


typedef struct wl_mtcpkeep_alive_timers_pkt {
	uint16 interval;		
	uint16 retry_interval;		
	uint16 retry_count;		
} wl_mtcpkeep_alive_timers_pkt_t;

typedef struct wake_info {
	uint32 wake_reason;
	uint32 wake_info_len;		
	uchar  packet[1];
} wake_info_t;

typedef struct wake_pkt {
	uint32 wake_pkt_len;		
	uchar  packet[1];
} wake_pkt_t;


#define WL_MTCPKEEP_ALIVE_VERSION		1

#ifdef WLBA

#define WLC_BA_CNT_VERSION  1   


typedef struct wlc_ba_cnt {
	uint16  version;    
	uint16  length;     

	
	uint32 txpdu;       
	uint32 txsdu;       
	uint32 txfc;        
	uint32 txfci;       
	uint32 txretrans;   
	uint32 txbatimer;   
	uint32 txdrop;      
	uint32 txaddbareq;  
	uint32 txaddbaresp; 
	uint32 txdelba;     
	uint32 txba;        
	uint32 txbar;       
	uint32 txpad[4];    

	
	uint32 rxpdu;       
	uint32 rxqed;       
	uint32 rxdup;       
	uint32 rxnobuf;     
	uint32 rxaddbareq;  
	uint32 rxaddbaresp; 
	uint32 rxdelba;     
	uint32 rxba;        
	uint32 rxbar;       
	uint32 rxinvba;     
	uint32 rxbaholes;   
	uint32 rxunexp;     
	uint32 rxpad[4];    
} wlc_ba_cnt_t;
#endif 


struct ampdu_tid_control {
	uint8 tid;			
	uint8 enable;			
};


struct ampdu_ea_tid {
	struct ether_addr ea;		
	uint8 tid;			
};

struct ampdu_retry_tid {
	uint8 tid;	
	uint8 retry;	
};


typedef struct dpt_iovar {
	struct ether_addr ea;		
	uint8 mode;			
	uint32 pad;			
} dpt_iovar_t;

#define	DPT_FNAME_LEN		48	

typedef struct dpt_status {
	uint8 status;			
	uint8 fnlen;			
	uchar name[DPT_FNAME_LEN];	
	uint32 rssi;			
	sta_info_t sta;			
} dpt_status_t;


typedef struct dpt_list {
	uint32 num;			
	dpt_status_t status[1];		
} dpt_list_t;


typedef struct dpt_fname {
	uint8 len;			
	uchar name[DPT_FNAME_LEN];	
} dpt_fname_t;

#define BDD_FNAME_LEN       32  
typedef struct bdd_fname {
	uint8 len;          
	uchar name[BDD_FNAME_LEN];  
} bdd_fname_t;



struct tslist {
	int count;			
	struct tsinfo_arg tsinfo[1];	
};

#ifdef WLTDLS

typedef struct tdls_iovar {
	struct ether_addr ea;		
	uint8 mode;			
	chanspec_t chanspec;
	uint32 pad;			
} tdls_iovar_t;

#define TDLS_WFD_IE_SIZE		512

typedef struct tdls_wfd_ie_iovar {
	struct ether_addr ea;		
	uint8 mode;
	uint16 length;
	uint8 data[TDLS_WFD_IE_SIZE];
} tdls_wfd_ie_iovar_t;
#endif 


typedef struct tspec_arg {
	uint16 version;			
	uint16 length;			
	uint flag;			
	
	struct tsinfo_arg tsinfo;	
	uint16 nom_msdu_size;		
	uint16 max_msdu_size;		
	uint min_srv_interval;		
	uint max_srv_interval;		
	uint inactivity_interval;	
	uint suspension_interval;	
	uint srv_start_time;		
	uint min_data_rate;		
	uint mean_data_rate;		
	uint peak_data_rate;		
	uint max_burst_size;		
	uint delay_bound;		
	uint min_phy_rate;		
	uint16 surplus_bw;		
	uint16 medium_time;		
	uint8 dialog_token;		
} tspec_arg_t;


typedef	struct tspec_per_sta_arg {
	struct ether_addr ea;
	struct tspec_arg ts;
} tspec_per_sta_arg_t;


typedef	struct wme_max_bandwidth {
	uint32	ac[AC_COUNT];	
} wme_max_bandwidth_t;

#define WL_WME_MBW_PARAMS_IO_BYTES (sizeof(wme_max_bandwidth_t))


#define	TSPEC_ARG_VERSION		2	
#define TSPEC_ARG_LENGTH		55	
#define TSPEC_DEFAULT_DIALOG_TOKEN	42	
#define TSPEC_DEFAULT_SBW_FACTOR	0x3000	


#define WL_WOWL_KEEPALIVE_MAX_PACKET_SIZE  80
#define WLC_WOWL_MAX_KEEPALIVE	2


typedef struct wl_lifetime {
	uint32 ac;	        
	uint32 lifetime;    
} wl_lifetime_t;


typedef struct wl_chan_switch {
	uint8 mode;		
	uint8 count;		
	chanspec_t chspec;	
	uint8 reg;		
	uint8 frame_type;		
} wl_chan_switch_t;

enum {
	PFN_LIST_ORDER,
	PFN_RSSI
};

enum {
	DISABLE,
	ENABLE
};

enum {
	OFF_ADAPT,
	SMART_ADAPT,
	STRICT_ADAPT,
	SLOW_ADAPT
};

#define SORT_CRITERIA_BIT		0
#define AUTO_NET_SWITCH_BIT		1
#define ENABLE_BKGRD_SCAN_BIT		2
#define IMMEDIATE_SCAN_BIT		3
#define	AUTO_CONNECT_BIT		4
#define	ENABLE_BD_SCAN_BIT		5
#define ENABLE_ADAPTSCAN_BIT		6
#define IMMEDIATE_EVENT_BIT		8
#define SUPPRESS_SSID_BIT		9
#define ENABLE_NET_OFFLOAD_BIT		10

#define REPORT_SEPERATELY_BIT		11

#define SORT_CRITERIA_MASK	0x0001
#define AUTO_NET_SWITCH_MASK	0x0002
#define ENABLE_BKGRD_SCAN_MASK	0x0004
#define IMMEDIATE_SCAN_MASK	0x0008
#define AUTO_CONNECT_MASK	0x0010

#define ENABLE_BD_SCAN_MASK	0x0020
#define ENABLE_ADAPTSCAN_MASK	0x00c0
#define IMMEDIATE_EVENT_MASK	0x0100
#define SUPPRESS_SSID_MASK	0x0200
#define ENABLE_NET_OFFLOAD_MASK	0x0400

#define REPORT_SEPERATELY_MASK	0x0800

#define PFN_VERSION			2
#define PFN_SCANRESULT_VERSION		1
#define MAX_PFN_LIST_COUNT		16

#define PFN_COMPLETE			1
#define PFN_INCOMPLETE			0

#define DEFAULT_BESTN			2
#define DEFAULT_MSCAN			0
#define DEFAULT_REPEAT			10
#define DEFAULT_EXP			2

#define PFN_PARTIAL_SCAN_BIT		0
#define PFN_PARTIAL_SCAN_MASK		1


typedef struct wl_pfn_subnet_info {
	struct ether_addr BSSID;
	uint8	channel; 
	uint8	SSID_len;
	uint8	SSID[32];
} wl_pfn_subnet_info_t;

typedef struct wl_pfn_net_info {
	wl_pfn_subnet_info_t pfnsubnet;
	int16	RSSI; 
	uint16	timestamp; 
} wl_pfn_net_info_t;

typedef struct wl_pfn_lnet_info {
	wl_pfn_subnet_info_t pfnsubnet; 
	uint16	flags; 
	int16	RSSI; 
	uint32	timestamp; 
	uint16	rtt0; 
	uint16	rtt1; 
} wl_pfn_lnet_info_t;

typedef struct wl_pfn_lscanresults {
	uint32 version;
	uint32 status;
	uint32 count;
	wl_pfn_lnet_info_t netinfo[1];
} wl_pfn_lscanresults_t;

typedef struct wl_pfn_scanresults {
	uint32 version;
	uint32 status;
	uint32 count;
	wl_pfn_net_info_t netinfo[1];
} wl_pfn_scanresults_t;


typedef struct wl_pfn_param {
	int32 version;			
	int32 scan_freq;		
	int32 lost_network_timeout;	
	int16 flags;			
	int16 rssi_margin;		
	uint8 bestn; 
	uint8 mscan; 
	uint8 repeat; 
	uint8 exp; 
#if !defined(WLC_PATCH) || !defined(BCM43362A2)
	int32 slow_freq; 
#endif 
} wl_pfn_param_t;

typedef struct wl_pfn_bssid {
	struct ether_addr  macaddr;
	
	uint16             flags;
} wl_pfn_bssid_t;
#define WL_PFN_SUPPRESSFOUND_MASK	0x08
#define WL_PFN_SUPPRESSLOST_MASK	0x10
#define WL_PFN_RSSI_MASK		0xff00
#define WL_PFN_RSSI_SHIFT		8

typedef struct wl_pfn_cfg {
	uint32	reporttype;
	int32	channel_num;
	uint16	channel_list[WL_NUMCHANNELS];
	uint32	flags;
} wl_pfn_cfg_t;
#define WL_PFN_REPORT_ALLNET    0
#define WL_PFN_REPORT_SSIDNET   1
#define WL_PFN_REPORT_BSSIDNET  2

#define WL_PFN_CFG_FLAGS_PROHIBITED	0x00000001	
#define WL_PFN_CFG_FLAGS_RESERVED	0xfffffffe	

typedef struct wl_pfn {
	wlc_ssid_t		ssid;			
	int32			flags;			
	int32			infra;			
	int32			auth;			
	int32			wpa_auth;		
	int32			wsec;			
} wl_pfn_t;

typedef struct wl_pfn_list {
	uint32		version;
	uint32		enabled;
	uint32		count;
	wl_pfn_t	pfn[1];
} wl_pfn_list_t;

typedef BWL_PRE_PACKED_STRUCT struct pfn_olmsg_params_t {
	wlc_ssid_t ssid;
	uint32	cipher_type;
	uint32	auth_type;
	uint8	channels[4];
} BWL_POST_PACKED_STRUCT pfn_olmsg_params;

#define WL_PFN_HIDDEN_BIT		2
#define WL_PFN_HIDDEN_MASK		0x4

#ifndef BESTN_MAX
#define BESTN_MAX			3
#endif

#ifndef MSCAN_MAX
#define MSCAN_MAX			90
#endif

#endif 


typedef struct {
	uint8	transaction_id;	
	uint8	protocol;	
	uint16	query_len;	
	uint16	response_len;	
	uint8	qrbuf[1];
} wl_p2po_qr_t;

typedef struct {
	uint16			period;			
	uint16			interval;		
} wl_p2po_listen_t;



#define ANQPO_MAX_QUERY_SIZE		256
typedef struct {
	uint16 max_retransmit;		
	uint16 response_timeout;	
	uint16 max_comeback_delay;	
	uint16 max_retries;			
	uint16 query_len;			
	uint8 query_data[1];		
} wl_anqpo_set_t;

typedef struct {
	uint16 channel;				
	struct ether_addr addr;		
} wl_anqpo_peer_t;

#define ANQPO_MAX_PEER_LIST			64
typedef struct {
	uint16 count;				
	wl_anqpo_peer_t peer[1];	
} wl_anqpo_peer_list_t;

#define ANQPO_MAX_IGNORE_SSID		64
typedef struct {
	bool is_clear;				
	uint16 count;				
	wlc_ssid_t ssid[1];			
} wl_anqpo_ignore_ssid_list_t;

#define ANQPO_MAX_IGNORE_BSSID		64
typedef struct {
	bool is_clear;				
	uint16 count;				
	struct ether_addr bssid[1];	
} wl_anqpo_ignore_bssid_list_t;

#ifndef LINUX_POSTMOGRIFY_REMOVAL

struct toe_ol_stats_t {
	
	uint32 tx_summed;

	
	uint32 tx_iph_fill;
	uint32 tx_tcp_fill;
	uint32 tx_udp_fill;
	uint32 tx_icmp_fill;

	
	uint32 rx_iph_good;
	uint32 rx_iph_bad;
	uint32 rx_tcp_good;
	uint32 rx_tcp_bad;
	uint32 rx_udp_good;
	uint32 rx_udp_bad;
	uint32 rx_icmp_good;
	uint32 rx_icmp_bad;

	
	uint32 tx_tcp_errinj;
	uint32 tx_udp_errinj;
	uint32 tx_icmp_errinj;

	
	uint32 rx_tcp_errinj;
	uint32 rx_udp_errinj;
	uint32 rx_icmp_errinj;
};


struct arp_ol_stats_t {
	uint32  host_ip_entries;	
	uint32  host_ip_overflow;	

	uint32  arp_table_entries;	
	uint32  arp_table_overflow;	

	uint32  host_request;		
	uint32  host_reply;		
	uint32  host_service;		

	uint32  peer_request;		
	uint32  peer_request_drop;	
	uint32  peer_reply;		
	uint32  peer_reply_drop;	
	uint32  peer_service;		
};


struct nd_ol_stats_t {
	uint32  host_ip_entries;    
	uint32  host_ip_overflow;   
	uint32  peer_request;       
	uint32  peer_request_drop;  
	uint32  peer_reply_drop;    
	uint32  peer_service;       
};




typedef struct wl_keep_alive_pkt {
	uint32	period_msec;	
	uint16	len_bytes;	
	uint8	data[1];	
} wl_keep_alive_pkt_t;

#define WL_KEEP_ALIVE_FIXED_LEN		OFFSETOF(wl_keep_alive_pkt_t, data)

typedef struct awdl_config_params {
	uint32	version;
	uint8	awdl_chan;		
	uint8	guard_time;		
	uint16	aw_period;		
	uint16  aw_cmn_length;		
	uint16	action_frame_period;	
	uint16  awdl_pktlifetime;	
	uint16  awdl_maxnomaster;	
	uint16  awdl_extcount;		
	uint16	aw_ext_length;		
	uint16	awdl_nmode;	        
	struct ether_addr ea;		
} awdl_config_params_t;

typedef struct wl_awdl_action_frame {
	uint16	len_bytes;
	uint8	awdl_action_frame_data[1];
} wl_awdl_action_frame_t;

#define WL_AWDL_ACTION_FRAME_FIXED_LEN		OFFSETOF(wl_awdl_action_frame_t, awdl_sync_frame)

typedef struct awdl_peer_node {
	uint32	type_state;		
	uint16	aw_counter;		
	int8	rssi;			
	int8	last_rssi;		
	uint16	tx_counter;
	uint16	tx_delay;		
	uint16	period_tu;
	uint16	aw_period;
	uint16	aw_cmn_length;
	uint16	aw_ext_length;
	uint32	self_metrics;		
	uint32	top_master_metrics;	
	struct ether_addr	addr;
	struct ether_addr	top_master;
	uint8	dist_top;		
} awdl_peer_node_t;

typedef struct awdl_peer_table {
	uint16  version;
	uint16	len;
	uint8 peer_nodes[1];
} awdl_peer_table_t;

typedef struct awdl_af_hdr {
	struct ether_addr dst_mac;
	uint8 action_hdr[4]; 
} awdl_af_hdr_t;

typedef struct awdl_oui {
	uint8 oui[3];	
	uint8 oui_type; 
} awdl_oui_t;

typedef BWL_PRE_PACKED_STRUCT struct awdl_hdr {
	uint8	type;		
	uint8	version;
	uint8	sub_type;	
	uint8	rsvd;		
	uint32	phy_timestamp;	
	uint32	fw_timestamp;	
} BWL_POST_PACKED_STRUCT awdl_hdr_t;

typedef BWL_PRE_PACKED_STRUCT struct awdl_oob_af_params {
	struct ether_addr bssid;
	struct ether_addr dst_mac;
	uint32 channel;
	uint32 dwell_time;
	uint32 flags;
	uint32 pkt_lifetime;
	uint32 tx_rate;
	uint32 max_retries; 
	uint16 payload_len;
	uint8  payload[1]; 
} BWL_POST_PACKED_STRUCT awdl_oob_af_params_t;

typedef BWL_PRE_PACKED_STRUCT struct awdl_sync_params {
	uint8	type;			
	uint16	param_len;		
	uint8	tx_chan;		
	uint16	tx_counter;		
	uint8	master_chan;		
	uint8	guard_time;		
	uint16	aw_period;		
	uint16	action_frame_period;	
	uint16	awdl_flags;		
	uint16	aw_ext_length;		
	uint16	aw_cmn_length;		
	uint16	aw_remaining;		
	uint8	min_ext;		
	uint8	max_ext_multi;		
	uint8	max_ext_uni;		
	uint8	max_ext_af;		
	struct ether_addr current_master;	
	uint8	presence_mode;		
	uint8	reserved;
	uint16	aw_counter;		
	uint16	ap_bcn_alignment_delta;	
} BWL_POST_PACKED_STRUCT awdl_sync_params_t;

typedef BWL_PRE_PACKED_STRUCT struct awdl_channel_sequence {
	uint8	aw_seq_len;		
	uint8	aw_seq_enc;		
	uint8	aw_seq_duplicate_cnt;	
	uint8	seq_step_cnt;		
	uint16	seq_fill_chan;		
	uint8	chan_sequence[1];	
} BWL_POST_PACKED_STRUCT awdl_channel_sequence_t;
#define WL_AWDL_CHAN_SEQ_FIXED_LEN   OFFSETOF(awdl_channel_sequence_t, chan_sequence)

typedef BWL_PRE_PACKED_STRUCT struct awdl_election_info {
	uint8	election_flags;	
	uint16	election_ID;	
	uint32	self_metrics;
} BWL_POST_PACKED_STRUCT awdl_election_info_t;

typedef BWL_PRE_PACKED_STRUCT struct awdl_election_tree_info {
	uint8	election_flags;	
	uint16	election_ID;	
	uint32	self_metrics;
	int8 master_sync_rssi_thld;
	int8 slave_sync_rssi_thld;
	int8 edge_sync_rssi_thld;
	int8 close_range_rssi_thld;
	int8 mid_range_rssi_thld;
	uint8 max_higher_masters_close_range;
	uint8 max_higher_masters_mid_range;
	uint8 max_tree_depth;
	
	struct ether_addr top_master;	
	uint32 top_master_self_metric;
	uint8  current_tree_depth;
} BWL_POST_PACKED_STRUCT awdl_election_tree_info_t;

typedef BWL_PRE_PACKED_STRUCT struct awdl_election_params_tlv {
	uint8	type;			
	uint16	param_len;		
	uint8	election_flags;	
	uint16	election_ID;	
	uint8	dist_top;	
	uint8	rsvd;		
	struct ether_addr top_master;	
	uint32	top_master_metrics;
	uint32	self_metrics;
	uint8	pad[2];		
} BWL_POST_PACKED_STRUCT awdl_election_params_tlv_t;

typedef struct awdl_payload {
	uint32	len;		
	uint8	payload[1];	
} awdl_payload_t;

typedef struct awdl_long_payload {
	uint8   long_psf_period;      
	uint8   long_psf_tx_offset;   
	uint16	len;		          
	uint8	payload[1];           
} BWL_POST_PACKED_STRUCT awdl_long_payload_t;

typedef BWL_PRE_PACKED_STRUCT struct awdl_opmode {
	uint8	mode;		
	uint8	role;		
	uint16	bcast_tu; 
	struct ether_addr master; 
	uint16	cur_bcast_tu;	
} BWL_PRE_PACKED_STRUCT awdl_opmode_t;

typedef BWL_PRE_PACKED_STRUCT struct awdl_extcount {
	uint8	minExt;			
	uint8	maxExtMulti;	
	uint8	maxExtUni;		
	uint8	maxAfExt;			
} BWL_PRE_PACKED_STRUCT awdl_extcount_t;


typedef struct awdl_peer_op {
	uint8 version;
	uint8 opcode;	
	struct ether_addr addr;
	uint8 mode;
} awdl_peer_op_t;


typedef struct awdl_peer_op_tbl {
	uint16	len;		
	uint8	tbl[1];	
} awdl_peer_op_tbl_t;

typedef BWL_PRE_PACKED_STRUCT struct awdl_peer_op_node {
	struct ether_addr addr;
	uint32 flags;	
} BWL_POST_PACKED_STRUCT awdl_peer_op_node_t;

#define AWDL_PEER_OP_CUR_VER	0


typedef BWL_PRE_PACKED_STRUCT struct awdl_stats {
	uint32	afrx;
	uint32	aftx;
	uint32	datatx;
	uint32	datarx;
	uint32	txdrop;
	uint32	rxdrop;
	uint32	monrx;
	uint32	lostmaster;
	uint32	misalign;
	uint32	aws;
	uint32	aw_dur;
	uint32	debug;
	uint32  txsupr;
	uint32	afrxdrop;
	uint32  awdrop;
	uint32  noawchansw;
	uint32  rx80211;
	uint32  peeropdrop;
} BWL_POST_PACKED_STRUCT awdl_stats_t;

typedef BWL_PRE_PACKED_STRUCT struct awdl_uct_stats {
	uint32 aw_proc_in_aw_sched;
	uint32 aw_upd_in_pre_aw_proc;
	uint32 pre_aw_proc_in_aw_set;
	uint32 ignore_pre_aw_proc;
	uint32 miss_pre_aw_intr;
	uint32 aw_dur_zero;
	uint32 aw_sched;
	uint32 aw_proc;
	uint32 pre_aw_proc;
	uint32 not_init;
	uint32 null_awdl;
} BWL_POST_PACKED_STRUCT awdl_uct_stats_t;

typedef struct awdl_pw_opmode {
	struct ether_addr top_master;	
	uint8 mode; 
} awdl_pw_opmode_t;


typedef struct wl_awdl_if {
	int32 cfg_idx;
	int32 up;
	struct ether_addr if_addr;
	struct ether_addr bssid;
} wl_awdl_if_t;

typedef struct _aw_start {
	uint8 role;
	struct ether_addr	master;
	uint8	aw_seq_num;
} aw_start_t;

typedef struct _aw_extension_start {
	uint8 aw_ext_num;
} aw_extension_start_t;

typedef struct _awdl_peer_state {
	struct ether_addr peer;
	uint8	state;
} awdl_peer_state_t;

typedef struct _awdl_sync_state_changed {
	uint8	new_role;
	struct ether_addr master;
} awdl_sync_state_changed_t;

typedef struct _awdl_sync_state {
	uint8	role;
	struct ether_addr master;
	uint32 continuous_election_enable;
} awdl_sync_state_t;

typedef struct _awdl_aw_ap_alignment {
	uint32	enabled;
	int32	offset;
	uint32	align_on_dtim;
} awdl_aw_ap_alignment_t;

typedef struct _awdl_peer_stats {
	uint32 version;
	struct ether_addr address;
	uint8 clear;
	int8 rssi;
	int8 avg_rssi;
	uint8 txRate;
	uint8 rxRate;
	uint32 numTx;
	uint32 numTxRetries;
	uint32 numTxFailures;
} awdl_peer_stats_t;

#define MAX_NUM_AWDL_KEYS 4
typedef struct _awdl_aes_key {
	uint32 version;
	int32 enable;
	struct ether_addr awdl_peer;
	uint8 keys[MAX_NUM_AWDL_KEYS][16];
} awdl_aes_key_t;



#define MAX_WAKE_PACKET_CACHE_BYTES 128 

#define MAX_WAKE_PACKET_BYTES	    (DOT11_A3_HDR_LEN +			    \
				     DOT11_QOS_LEN +			    \
				     sizeof(struct dot11_llc_snap_header) + \
				     ETHER_MAX_DATA)

typedef struct pm_wake_packet {
	uint32	status;		
	uint32	pattern_id;	
	uint32	original_packet_size;
	uint32	saved_packet_size;
	uchar	packet[MAX_WAKE_PACKET_CACHE_BYTES];
} pm_wake_packet_t;


typedef enum wl_pkt_filter_type {
	WL_PKT_FILTER_TYPE_PATTERN_MATCH=0,	
	WL_PKT_FILTER_TYPE_MAGIC_PATTERN_MATCH=1, 
	WL_PKT_FILTER_TYPE_PATTERN_LIST_MATCH=2	
} wl_pkt_filter_type_t;

#define WL_PKT_FILTER_TYPE wl_pkt_filter_type_t


#define WL_PKT_FILTER_TYPE_NAMES \
	{ "PATTERN", WL_PKT_FILTER_TYPE_PATTERN_MATCH },       \
	{ "MAGIC",   WL_PKT_FILTER_TYPE_MAGIC_PATTERN_MATCH }, \
	{ "PATLIST", WL_PKT_FILTER_TYPE_PATTERN_LIST_MATCH }


typedef struct wl_pkt_filter_pattern {
	uint32	offset;		
	uint32	size_bytes;	
	uint8   mask_and_pattern[1]; 
} wl_pkt_filter_pattern_t;


typedef struct wl_pkt_filter_pattern_listel {
	uint16 rel_offs;	
	uint16 base_offs;	
	uint16 size_bytes;	
	uint16 match_flags;	
	uint8  mask_and_data[1]; 
} wl_pkt_filter_pattern_listel_t;

typedef struct wl_pkt_filter_pattern_list {
	uint8 list_cnt;		
	uint8 PAD1[1];		
	uint16 totsize;		
	wl_pkt_filter_pattern_listel_t patterns[1]; 
} wl_pkt_filter_pattern_list_t;


typedef struct wl_pkt_filter {
	uint32	id;		
	uint32	type;		
	uint32	negate_match;	
	union {			
		wl_pkt_filter_pattern_t pattern;	
		wl_pkt_filter_pattern_list_t patlist; 
	} u;
} wl_pkt_filter_t;


typedef struct wl_tcp_keep_set {
	uint32	val1;
	uint32	val2;
} wl_tcp_keep_set_t;

#define WL_PKT_FILTER_FIXED_LEN		  OFFSETOF(wl_pkt_filter_t, u)
#define WL_PKT_FILTER_PATTERN_FIXED_LEN	  OFFSETOF(wl_pkt_filter_pattern_t, mask_and_pattern)
#define WL_PKT_FILTER_PATTERN_LIST_FIXED_LEN OFFSETOF(wl_pkt_filter_pattern_list_t, patterns)
#define WL_PKT_FILTER_PATTERN_LISTEL_FIXED_LEN	\
			OFFSETOF(wl_pkt_filter_pattern_listel_t, mask_and_data)


typedef struct wl_pkt_filter_enable {
	uint32	id;		
	uint32	enable;		
} wl_pkt_filter_enable_t;


typedef struct wl_pkt_filter_list {
	uint32	num;		
	wl_pkt_filter_t	filter[1];	
} wl_pkt_filter_list_t;

#define WL_PKT_FILTER_LIST_FIXED_LEN	  OFFSETOF(wl_pkt_filter_list_t, filter)


typedef struct wl_pkt_filter_stats {
	uint32	num_pkts_matched;	
	uint32	num_pkts_forwarded;	
	uint32	num_pkts_discarded;	
} wl_pkt_filter_stats_t;


typedef struct wl_pkt_filter_ports {
	uint8 version;		
	uint8 reserved;		
	uint16 count;		
	
	uint16 ports[1];	
} wl_pkt_filter_ports_t;

#define WL_PKT_FILTER_PORTS_FIXED_LEN	OFFSETOF(wl_pkt_filter_ports_t, ports)

#define WL_PKT_FILTER_PORTS_VERSION	0
#define WL_PKT_FILTER_PORTS_MAX		128

#define RSN_KCK_LENGTH 16
#define RSN_KEK_LENGTH 16
#define RSN_REPLAY_LEN 8
typedef struct _gtkrefresh {
	uchar	KCK[RSN_KCK_LENGTH];
	uchar	KEK[RSN_KEK_LENGTH];
	uchar	ReplayCounter[RSN_REPLAY_LEN];
} gtk_keyinfo_t, *pgtk_keyinfo_t;


typedef struct wl_seq_cmd_ioctl {
	uint32 cmd;		
	uint32 len;		
} wl_seq_cmd_ioctl_t;

#define WL_SEQ_CMD_ALIGN_BYTES	4


#define WL_SEQ_CMDS_GET_IOCTL_FILTER(cmd) \
	(((cmd) == WLC_GET_MAGIC)		|| \
	 ((cmd) == WLC_GET_VERSION)		|| \
	 ((cmd) == WLC_GET_AP)			|| \
	 ((cmd) == WLC_GET_INSTANCE))

typedef struct wl_pkteng {
	uint32 flags;
	uint32 delay;			
	uint32 nframes;			
	uint32 length;			
	uint8  seqno;			
	struct ether_addr dest;		
	struct ether_addr src;		
} wl_pkteng_t;

typedef struct wl_pkteng_stats {
	uint32 lostfrmcnt;		
	int32 rssi;			
	int32 snr;			
	uint16 rxpktcnt[NUM_80211_RATES+1];
	uint8 rssi_qdb;			
} wl_pkteng_stats_t;


typedef enum {
	wowl_pattern_type_bitmap = 0,
	wowl_pattern_type_arp,
	wowl_pattern_type_na
} wowl_pattern_type_t;

typedef struct wl_wowl_pattern {
	uint32		    masksize;		
	uint32		    offset;		
	uint32		    patternoffset;	
	uint32		    patternsize;	
	uint32		    id;			
	uint32		    reasonsize;		
	wowl_pattern_type_t type;		
	
	
} wl_wowl_pattern_t;

typedef struct wl_wowl_pattern_list {
	uint			count;
	wl_wowl_pattern_t	pattern[1];
} wl_wowl_pattern_list_t;

typedef struct wl_wowl_wakeind {
	uint8	pci_wakeind;	
	uint32	ucode_wakeind;	
} wl_wowl_wakeind_t;

typedef struct {
	uint32		pktlen;		    
	void		*sdu;
} tcp_keepalive_wake_pkt_infop_t;


typedef struct wl_txrate_class {
	uint8		init_rate;
	uint8		min_rate;
	uint8		max_rate;
} wl_txrate_class_t;


typedef struct wl_obss_scan_arg {
	int16	passive_dwell;
	int16	active_dwell;
	int16	bss_widthscan_interval;
	int16	passive_total;
	int16	active_total;
	int16	chanwidth_transition_delay;
	int16	activity_threshold;
} wl_obss_scan_arg_t;

#define WL_OBSS_SCAN_PARAM_LEN	sizeof(wl_obss_scan_arg_t)


typedef struct wl_rssi_event {
	uint32 rate_limit_msec;		
	uint8 num_rssi_levels;		
	int8 rssi_levels[MAX_RSSI_LEVELS];	
} wl_rssi_event_t;

typedef struct wl_action_obss_coex_req {
	uint8 info;
	uint8 num;
	uint8 ch_list[1];
} wl_action_obss_coex_req_t;



#define WL_IOV_MAC_PARAM_LEN  4

#define WL_IOV_PKTQ_LOG_PRECS 16

typedef struct {
	uint32 num_addrs;
	char   addr_type[WL_IOV_MAC_PARAM_LEN];
	struct ether_addr ea[WL_IOV_MAC_PARAM_LEN];
} wl_iov_mac_params_t;


typedef struct {
	uint32 addr_info[WL_IOV_MAC_PARAM_LEN];
} wl_iov_mac_extra_params_t;


typedef struct {
	wl_iov_mac_params_t params;
	wl_iov_mac_extra_params_t extra_params;
} wl_iov_mac_full_params_t;


#define PKTQ_LOG_COUNTERS_V4 \
	 \
	uint32 requested; \
	 \
	uint32 stored; \
	 \
	uint32 saved; \
	 \
	uint32 selfsaved; \
	 \
	uint32 full_dropped; \
	  \
	uint32 dropped; \
	 \
	uint32 sacrificed; \
	 \
	uint32 busy; \
	 \
	uint32 retry; \
	 \
	uint32 ps_retry; \
	  \
	uint32 suppress; \
	 \
	uint32 retry_drop; \
	 \
	uint32 max_avail; \
	 \
	uint32 max_used; \
	  \
	uint32 queue_capacity; \
	 \
	uint32 rtsfail; \
	 \
	uint32 acked; \
	 \
	uint32 txrate_succ; \
	 \
	uint32 txrate_main; \
	 \
	uint32 throughput; \
	 \
	uint32 time_delta;

typedef struct {
	PKTQ_LOG_COUNTERS_V4
} pktq_log_counters_v04_t;


typedef struct {
	PKTQ_LOG_COUNTERS_V4
	
	uint32 airtime;
} pktq_log_counters_v05_t;

typedef struct {
	uint8                num_prec[WL_IOV_MAC_PARAM_LEN];
	pktq_log_counters_v04_t  counters[WL_IOV_MAC_PARAM_LEN][WL_IOV_PKTQ_LOG_PRECS];
	uint32               counter_info[WL_IOV_MAC_PARAM_LEN];
	uint32               pspretend_time_delta[WL_IOV_MAC_PARAM_LEN];
	char                 headings[1];
} pktq_log_format_v04_t;

typedef struct {
	uint8                num_prec[WL_IOV_MAC_PARAM_LEN];
	pktq_log_counters_v05_t  counters[WL_IOV_MAC_PARAM_LEN][WL_IOV_PKTQ_LOG_PRECS];
	uint32               counter_info[WL_IOV_MAC_PARAM_LEN];
	uint32               pspretend_time_delta[WL_IOV_MAC_PARAM_LEN];
	char                 headings[1];
} pktq_log_format_v05_t;


typedef struct {
	uint32               version;
	wl_iov_mac_params_t  params;
	union {
		pktq_log_format_v04_t v04;
		pktq_log_format_v05_t v05;
	} pktq_log;
} wl_iov_pktq_log_t;


#define PKTQ_LOG_AUTO     (1 << 31)
#define PKTQ_LOG_DEF_PREC (1 << 30)


#define SCB_BS_DATA_STRUCT_VERSION	1


typedef BWL_PRE_PACKED_STRUCT struct {
	
	uint32 retry;          
	uint32 retry_drop;     
	uint32 rtsfail;        
	uint32 acked;          
	uint32 txrate_succ;    
	uint32 txrate_main;    
	uint32 throughput;     
	uint32 time_delta;     
	uint32 airtime;        
} BWL_POST_PACKED_STRUCT iov_bs_data_counters_t;


typedef BWL_PRE_PACKED_STRUCT struct {
	struct ether_addr	station_address;	
	uint16			station_flags;		
	iov_bs_data_counters_t	station_counters;	
} BWL_POST_PACKED_STRUCT iov_bs_data_record_t;

typedef BWL_PRE_PACKED_STRUCT struct {
	uint16	structure_version;	
	uint16	structure_count;	
	iov_bs_data_record_t	structure_record[1];	
} BWL_POST_PACKED_STRUCT iov_bs_data_struct_t;


enum {
	SCB_BS_DATA_FLAG_NO_RESET = (1<<0)	
};


typedef struct wlc_extlog_cfg {
	int max_number;
	uint16 module;	
	uint8 level;
	uint8 flag;
	uint16 version;
} wlc_extlog_cfg_t;

typedef struct log_record {
	uint32 time;
	uint16 module;
	uint16 id;
	uint8 level;
	uint8 sub_unit;
	uint8 seq_num;
	int32 arg;
	char str[MAX_ARGSTR_LEN];
} log_record_t;

typedef struct wlc_extlog_req {
	uint32 from_last;
	uint32 num;
} wlc_extlog_req_t;

typedef struct wlc_extlog_results {
	uint16 version;
	uint16 record_len;
	uint32 num;
	log_record_t logs[1];
} wlc_extlog_results_t;

typedef struct log_idstr {
	uint16	id;
	uint16	flag;
	uint8	arg_type;
	const char	*fmt_str;
} log_idstr_t;

#define FMTSTRF_USER		1


typedef enum {
	FMTSTR_DRIVER_UP_ID = 0,
	FMTSTR_DRIVER_DOWN_ID = 1,
	FMTSTR_SUSPEND_MAC_FAIL_ID = 2,
	FMTSTR_NO_PROGRESS_ID = 3,
	FMTSTR_RFDISABLE_ID = 4,
	FMTSTR_REG_PRINT_ID = 5,
	FMTSTR_EXPTIME_ID = 6,
	FMTSTR_JOIN_START_ID = 7,
	FMTSTR_JOIN_COMPLETE_ID = 8,
	FMTSTR_NO_NETWORKS_ID = 9,
	FMTSTR_SECURITY_MISMATCH_ID = 10,
	FMTSTR_RATE_MISMATCH_ID = 11,
	FMTSTR_AP_PRUNED_ID = 12,
	FMTSTR_KEY_INSERTED_ID = 13,
	FMTSTR_DEAUTH_ID = 14,
	FMTSTR_DISASSOC_ID = 15,
	FMTSTR_LINK_UP_ID = 16,
	FMTSTR_LINK_DOWN_ID = 17,
	FMTSTR_RADIO_HW_OFF_ID = 18,
	FMTSTR_RADIO_HW_ON_ID = 19,
	FMTSTR_EVENT_DESC_ID = 20,
	FMTSTR_PNP_SET_POWER_ID = 21,
	FMTSTR_RADIO_SW_OFF_ID = 22,
	FMTSTR_RADIO_SW_ON_ID = 23,
	FMTSTR_PWD_MISMATCH_ID = 24,
	FMTSTR_FATAL_ERROR_ID = 25,
	FMTSTR_AUTH_FAIL_ID = 26,
	FMTSTR_ASSOC_FAIL_ID = 27,
	FMTSTR_IBSS_FAIL_ID = 28,
	FMTSTR_EXTAP_FAIL_ID = 29,
	FMTSTR_MAX_ID
} log_fmtstr_id_t;

#ifdef DONGLEOVERLAYS
typedef struct {
	uint32 flags_idx;	
	uint32 offset;		
	uint32 len;			
	
} wl_ioctl_overlay_t;
#endif 

#endif 


typedef struct nbr_element {
	uint8 id;
	uint8 len;
	struct ether_addr bssid;
	uint32 bssid_info;
	uint8 reg;
	uint8 channel;
	uint8 phytype;
	uint8 pad;
} nbr_element_t;


#include <packed_section_end.h>

typedef struct keepalives_max_idle {
	uint16  keepalive_count;        
	uint8   mkeepalive_index;       
	uint8   PAD;			
	uint16  max_interval;           
} keepalives_max_idle_t;

#define PM_IGNORE_BCMC_PROXY_ARP (1 << 0)
#define PM_IGNORE_BCMC_ALL_DMS_ACCEPTED (1 << 1)


#include <packed_section_start.h>

#ifndef LINUX_POSTMOGRIFY_REMOVAL


#define VNDR_IE_CMD_LEN		4	

#define VNDR_IE_INFO_HDR_LEN	(sizeof(uint32))

typedef BWL_PRE_PACKED_STRUCT struct {
	uint32 pktflag;			
	vndr_ie_t vndr_ie_data;		
} BWL_POST_PACKED_STRUCT vndr_ie_info_t;

typedef BWL_PRE_PACKED_STRUCT struct {
	int iecount;			
	vndr_ie_info_t vndr_ie_list[1];	
} BWL_POST_PACKED_STRUCT vndr_ie_buf_t;

typedef BWL_PRE_PACKED_STRUCT struct {
	char cmd[VNDR_IE_CMD_LEN];	
	vndr_ie_buf_t vndr_ie_buffer;	
} BWL_POST_PACKED_STRUCT vndr_ie_setbuf_t;


typedef BWL_PRE_PACKED_STRUCT struct {
	uint8	id;
	uint8	len;
	uint8	data[1];
} BWL_POST_PACKED_STRUCT tlv_t;

typedef BWL_PRE_PACKED_STRUCT struct {
	uint32 pktflag;			
	tlv_t ie_data;		
} BWL_POST_PACKED_STRUCT ie_info_t;

typedef BWL_PRE_PACKED_STRUCT struct {
	int iecount;			
	ie_info_t ie_list[1];	
} BWL_POST_PACKED_STRUCT ie_buf_t;

typedef BWL_PRE_PACKED_STRUCT struct {
	char cmd[VNDR_IE_CMD_LEN];	
	ie_buf_t ie_buffer;	
} BWL_POST_PACKED_STRUCT ie_setbuf_t;

typedef BWL_PRE_PACKED_STRUCT struct {
	uint32 pktflag;		
	uint8 id;		
} BWL_POST_PACKED_STRUCT ie_getbuf_t;



typedef BWL_PRE_PACKED_STRUCT struct sta_prbreq_wps_ie_hdr {
	struct ether_addr staAddr;
	uint16 ieLen;
} BWL_POST_PACKED_STRUCT sta_prbreq_wps_ie_hdr_t;

typedef BWL_PRE_PACKED_STRUCT struct sta_prbreq_wps_ie_data {
	sta_prbreq_wps_ie_hdr_t hdr;
	uint8 ieData[1];
} BWL_POST_PACKED_STRUCT sta_prbreq_wps_ie_data_t;

typedef BWL_PRE_PACKED_STRUCT struct sta_prbreq_wps_ie_list {
	uint32 totLen;
	uint8 ieDataList[1];
} BWL_POST_PACKED_STRUCT sta_prbreq_wps_ie_list_t;


#ifdef WLMEDIA_TXFAILEVENT
typedef BWL_PRE_PACKED_STRUCT struct {
	char   dest[ETHER_ADDR_LEN]; 
	uint8  prio;            
	uint8  flags;           
	uint32 tsf_l;           
	uint32 tsf_h;           
	uint16 rates;           
	uint16 txstatus;        
} BWL_POST_PACKED_STRUCT txfailinfo_t;
#endif 

typedef BWL_PRE_PACKED_STRUCT struct {
	uint32 flags;
	chanspec_t chanspec;			
	chanspec_t local_chanspec;		
	uint8 local_max;			
	uint8 local_constraint;			
	int8  antgain[2];			
	uint8 rf_cores;				
	uint8 est_Pout[4];			
	uint8 est_Pout_act[4]; 
	uint8 est_Pout_cck;			
	uint8 tx_power_max[4];		
	uint tx_power_max_rate_ind[4];		
	int8 clm_limits[WL_NUMRATES];		
	int8 clm_limits_subchan1[WL_NUMRATES];	
	int8 clm_limits_subchan2[WL_NUMRATES];	
	int8 sar;					
	int8 channel_bandwidth;		
	uint8 version;				
	uint8 display_core;			
	int8 target_offsets[4];		
	uint32 last_tx_ratespec;	
	uint   user_target;		
	uint32 board_limit_len;		
	uint32 target_len;		
	int8 SARLIMIT[MAX_STREAMS_SUPPORTED];
	uint8  pprdata[1];		
} BWL_POST_PACKED_STRUCT tx_pwr_rpt_t;

typedef BWL_PRE_PACKED_STRUCT struct {
	struct ipv4_addr	ipv4_addr;
	struct ether_addr nexthop;
} BWL_POST_PACKED_STRUCT ibss_route_entry_t;
typedef BWL_PRE_PACKED_STRUCT struct {
	uint32 num_entry;
	ibss_route_entry_t route_entry[1];
} BWL_POST_PACKED_STRUCT ibss_route_tbl_t;

#define MAX_IBSS_ROUTE_TBL_ENTRY	64
#endif 

#define TXPWR_TARGET_VERSION  0
typedef BWL_PRE_PACKED_STRUCT struct {
	int32 version;		
	chanspec_t chanspec;	
	int8 txpwr[WL_STA_ANT_MAX]; 
	uint8 rf_cores;		
} BWL_POST_PACKED_STRUCT txpwr_target_max_t;

#define BSS_PEER_INFO_PARAM_CUR_VER	0

typedef BWL_PRE_PACKED_STRUCT	struct {
	uint16			version;
	struct	ether_addr ea;	
} BWL_POST_PACKED_STRUCT bss_peer_info_param_t;

#define BSS_PEER_INFO_CUR_VER		0

typedef BWL_PRE_PACKED_STRUCT struct {
	uint16			version;
	struct ether_addr	ea;
	int32			rssi;
	uint32			tx_rate;	
	uint32			rx_rate;	
	wl_rateset_t		rateset;	
	uint32			age;		
} BWL_POST_PACKED_STRUCT bss_peer_info_t;

#define BSS_PEER_LIST_INFO_CUR_VER	0

typedef BWL_PRE_PACKED_STRUCT struct {
	uint16			version;
	uint16			bss_peer_info_len;	
	uint32			count;			
	bss_peer_info_t		peer_info[1];		
} BWL_POST_PACKED_STRUCT bss_peer_list_info_t;

#define BSS_PEER_LIST_INFO_FIXED_LEN OFFSETOF(bss_peer_list_info_t, peer_info)

#define AIBSS_BCN_FORCE_CONFIG_VER_0	0


typedef BWL_PRE_PACKED_STRUCT struct {
	uint16  version;
	uint16	len;
	uint32 initial_min_bcn_dur;	
	uint32 min_bcn_dur;	
	uint32 bcn_flood_dur; 
} BWL_POST_PACKED_STRUCT aibss_bcn_force_config_t;

#define AIBSS_TXFAIL_CONFIG_VER_0    0


typedef BWL_PRE_PACKED_STRUCT struct {
	uint16  version;
	uint16  len;
	uint32 bcn_timeout;     
	uint32 max_tx_retry;     
} BWL_POST_PACKED_STRUCT aibss_txfail_config_t;


#include <packed_section_end.h>

#ifndef LINUX_POSTMOGRIFY_REMOVAL
	
#define ASSERTLOG_CUR_VER	0x0100
#define MAX_ASSRTSTR_LEN	64

	typedef struct assert_record {
		uint32 time;
		uint8 seq_num;
		char str[MAX_ASSRTSTR_LEN];
	} assert_record_t;

	typedef struct assertlog_results {
		uint16 version;
		uint16 record_len;
		uint32 num;
		assert_record_t logs[1];
	} assertlog_results_t;

#define LOGRRC_FIX_LEN	8
#define IOBUF_ALLOWED_NUM_OF_LOGREC(type, len) ((len - LOGRRC_FIX_LEN)/sizeof(type))


	
	typedef struct {
		bool valid;
		uint8 trigger;
		chanspec_t selected_chspc;
		int8 bgnoise;
		uint32 glitch_cnt;
		uint8 ccastats;
		uint timestamp;
	} chanim_acs_record_t;

	typedef struct {
		chanim_acs_record_t acs_record[CHANIM_ACS_RECORD];
		uint8 count;
		uint timestamp;
	} wl_acs_record_t;

	typedef struct chanim_stats {
		uint32 glitchcnt;               
		uint32 badplcp;                 
		uint8 ccastats[CCASTATS_MAX];   
		int8 bgnoise;			
		chanspec_t chanspec;
		uint32 timestamp;
		uint32 bphy_glitchcnt;          
		uint32 bphy_badplcp;            
		uint8 chan_idle;                
	} chanim_stats_t;

#define WL_CHANIM_STATS_VERSION 2

typedef struct {
	uint32 buflen;
	uint32 version;
	uint32 count;
	chanim_stats_t stats[1];
} wl_chanim_stats_t;

#define WL_CHANIM_STATS_FIXED_LEN OFFSETOF(wl_chanim_stats_t, stats)


#define NOISE_MEASURE_KNOISE	0x1


typedef struct {
	uint32 scb_timeout;
	uint32 scb_activity_time;
	uint32 scb_max_probe;
} wl_scb_probe_t;



#define SMFS_VERSION 1

typedef struct wl_smfs_elem {
	uint32 count;
	uint16 code;  
} wl_smfs_elem_t;

typedef struct wl_smf_stats {
	uint32 version;
	uint16 length;	
	uint8 type;
	uint8 codetype;
	uint32 ignored_cnt;
	uint32 malformed_cnt;
	uint32 count_total; 
	wl_smfs_elem_t elem[1];
} wl_smf_stats_t;

#define WL_SMFSTATS_FIXED_LEN OFFSETOF(wl_smf_stats_t, elem);

enum {
	SMFS_CODETYPE_SC,
	SMFS_CODETYPE_RC
};

typedef enum smfs_type {
	SMFS_TYPE_AUTH,
	SMFS_TYPE_ASSOC,
	SMFS_TYPE_REASSOC,
	SMFS_TYPE_DISASSOC_TX,
	SMFS_TYPE_DISASSOC_RX,
	SMFS_TYPE_DEAUTH_TX,
	SMFS_TYPE_DEAUTH_RX,
	SMFS_TYPE_MAX
} smfs_type_t;

#ifdef PHYMON

#define PHYMON_VERSION 1

typedef struct wl_phycal_core_state {
	
	int16 tx_iqlocal_a;
	int16 tx_iqlocal_b;
	int8 tx_iqlocal_ci;
	int8 tx_iqlocal_cq;
	int8 tx_iqlocal_di;
	int8 tx_iqlocal_dq;
	int8 tx_iqlocal_ei;
	int8 tx_iqlocal_eq;
	int8 tx_iqlocal_fi;
	int8 tx_iqlocal_fq;

	
	int16 rx_iqcal_a;
	int16 rx_iqcal_b;

	uint8 tx_iqlocal_pwridx; 
	uint32 papd_epsilon_table[64]; 
	int16 papd_epsilon_offset; 
	uint8 curr_tx_pwrindex; 
	int8 idle_tssi; 
	int8 est_tx_pwr; 
	int8 est_rx_pwr; 
	uint16 rx_gaininfo; 
	uint16 init_gaincode; 
	int8 estirr_tx;
	int8 estirr_rx;

} wl_phycal_core_state_t;

typedef struct wl_phycal_state {
	int version;
	int8 num_phy_cores; 
	int8 curr_temperature; 
	chanspec_t chspec; 
	bool aci_state; 
	uint16 crsminpower; 
	uint16 crsminpowerl; 
	uint16 crsminpoweru; 
	wl_phycal_core_state_t phycal_core[1];
} wl_phycal_state_t;

#define WL_PHYCAL_STAT_FIXED_LEN OFFSETOF(wl_phycal_state_t, phycal_core)
#endif 


typedef struct wl_p2p_disc_st {
	uint8 state;	
	chanspec_t chspec;	
	uint16 dwell;	
} wl_p2p_disc_st_t;


typedef struct wl_p2p_scan {
	uint8 type;		
	uint8 reserved[3];
	
} wl_p2p_scan_t;


typedef struct wl_p2p_if {
	struct ether_addr addr;
	uint8 type;	
	chanspec_t chspec;	
} wl_p2p_if_t;


typedef struct wl_p2p_ifq {
	uint bsscfgidx;
	char ifname[BCM_MSG_IFNAME_MAX];
} wl_p2p_ifq_t;


typedef struct wl_p2p_ops {
	uint8 ops;	
	uint8 ctw;	
} wl_p2p_ops_t;


typedef struct wl_p2p_sched_desc {
	uint32 start;
	uint32 interval;
	uint32 duration;
	uint32 count;	
} wl_p2p_sched_desc_t;

typedef struct wl_p2p_sched {
	uint8 type;	
	uint8 action;	
	uint8 option;	
	wl_p2p_sched_desc_t desc[1];
} wl_p2p_sched_t;

typedef struct wl_bcmdcs_data {
	uint reason;
	chanspec_t chspec;
} wl_bcmdcs_data_t;



typedef struct {
	uint32 ipaddr;		
	uint32 ipaddr_mask;	
	uint32 ipaddr_gateway;	
	uint8 mac_gateway[6];	
	uint32 ipaddr_dns;	
	uint8 mac_dns[6];	
	uint8 GUID[38];		
} nat_if_info_t;

typedef struct {
	uint op;		
	bool pub_if;		
	nat_if_info_t if_info;	
} nat_cfg_t;

typedef struct {
	int state;	
} nat_state_t;


#define BTA_STATE_LOG_SZ	64


enum {
	HCIReset = 1,
	HCIReadLocalAMPInfo,
	HCIReadLocalAMPASSOC,
	HCIWriteRemoteAMPASSOC,
	HCICreatePhysicalLink,
	HCIAcceptPhysicalLinkRequest,
	HCIDisconnectPhysicalLink,
	HCICreateLogicalLink,
	HCIAcceptLogicalLink,
	HCIDisconnectLogicalLink,
	HCILogicalLinkCancel,
	HCIAmpStateChange,
	HCIWriteLogicalLinkAcceptTimeout
};

typedef struct flush_txfifo {
	uint32 txfifobmp;
	uint32 hwtxfifoflush;
	struct ether_addr ea;
} flush_txfifo_t;

enum {
	SPATIAL_MODE_2G_IDX = 0,
	SPATIAL_MODE_5G_LOW_IDX,
	SPATIAL_MODE_5G_MID_IDX,
	SPATIAL_MODE_5G_HIGH_IDX,
	SPATIAL_MODE_5G_UPPER_IDX,
	SPATIAL_MODE_MAX_IDX
};

#define WLC_TXCORE_MAX	4	
#define WLC_SUBBAND_MAX	4	
typedef struct {
	uint8	band2g[WLC_TXCORE_MAX];
	uint8	band5g[WLC_SUBBAND_MAX][WLC_TXCORE_MAX];
} sar_limit_t;


typedef struct wl_mempool_stats {
	int	num;		
	bcm_mp_stats_t s[1];	
} wl_mempool_stats_t;

typedef struct {
	uint32 ipaddr;
	uint32 ipaddr_netmask;
	uint32 ipaddr_gateway;
} nwoe_ifconfig_t;


typedef enum trf_mgmt_priority_class {
	trf_mgmt_priority_low           = 0,        
	trf_mgmt_priority_medium        = 1,        
	trf_mgmt_priority_high          = 2,        
	trf_mgmt_priority_nochange	= 3,	    
	trf_mgmt_priority_invalid       = (trf_mgmt_priority_nochange + 1)
} trf_mgmt_priority_class_t;


typedef struct trf_mgmt_config {
	uint32  trf_mgmt_enabled;                           
	uint32  flags;                                      
	uint32  host_ip_addr;                               
	uint32  host_subnet_mask;                           
	uint32  downlink_bandwidth;                         
	uint32  uplink_bandwidth;                           
	uint32  min_tx_bandwidth[TRF_MGMT_MAX_PRIORITIES];  
	uint32  min_rx_bandwidth[TRF_MGMT_MAX_PRIORITIES];  
} trf_mgmt_config_t;


typedef struct trf_mgmt_filter {
	struct ether_addr           dst_ether_addr;         
	uint32                      dst_ip_addr;            
	uint16                      dst_port;               
	uint16                      src_port;               
	uint16                      prot;                   
	uint16                      flags;                  
	trf_mgmt_priority_class_t   priority;               
	uint32                      dscp;                   
} trf_mgmt_filter_t;


typedef struct trf_mgmt_filter_list     {
	uint32              num_filters;
	trf_mgmt_filter_t   filter[1];
} trf_mgmt_filter_list_t;


typedef struct trf_mgmt_global_info {
	uint32  maximum_bytes_per_second;
	uint32  maximum_bytes_per_sampling_period;
	uint32  total_bytes_consumed_per_second;
	uint32  total_bytes_consumed_per_sampling_period;
	uint32  total_unused_bytes_per_sampling_period;
} trf_mgmt_global_info_t;


typedef struct trf_mgmt_shaping_info {
	uint32  gauranteed_bandwidth_percentage;
	uint32  guaranteed_bytes_per_second;
	uint32  guaranteed_bytes_per_sampling_period;
	uint32  num_bytes_produced_per_second;
	uint32  num_bytes_consumed_per_second;
	uint32  num_queued_packets;                         
	uint32  num_queued_bytes;                           
} trf_mgmt_shaping_info_t;


typedef struct trf_mgmt_shaping_info_array {
	trf_mgmt_global_info_t   tx_global_shaping_info;
	trf_mgmt_shaping_info_t  tx_queue_shaping_info[TRF_MGMT_MAX_PRIORITIES];
	trf_mgmt_global_info_t   rx_global_shaping_info;
	trf_mgmt_shaping_info_t  rx_queue_shaping_info[TRF_MGMT_MAX_PRIORITIES];
} trf_mgmt_shaping_info_array_t;



typedef struct trf_mgmt_stats {
	uint32  num_processed_packets;      
	uint32  num_processed_bytes;        
	uint32  num_discarded_packets;      
} trf_mgmt_stats_t;


typedef struct trf_mgmt_stats_array {
	trf_mgmt_stats_t  tx_queue_stats[TRF_MGMT_MAX_PRIORITIES];
	trf_mgmt_stats_t  rx_queue_stats[TRF_MGMT_MAX_PRIORITIES];
} trf_mgmt_stats_array_t;

typedef struct powersel_params {
	
	int32		tp_ratio_thresh;  
	uint8		rate_stab_thresh; 
	uint8		pwr_stab_thresh; 
	uint8		pwr_sel_exp_time; 
} powersel_params_t;

typedef struct lpc_params {
	
	uint8		rate_stab_thresh; 
	uint8		pwr_stab_thresh; 
	uint8		lpc_exp_time; 
	uint8		pwrup_slow_step; 
	uint8		pwrup_fast_step; 
	uint8		pwrdn_slow_step; 
} lpc_params_t;


#define	SCB_RETRY_SHORT_DEF	7	
#define WLPKTDLY_HIST_NBINS	16	


typedef struct scb_delay_stats {
	uint32 txmpdu_lost;	
	uint32 txmpdu_cnt[SCB_RETRY_SHORT_DEF]; 
	uint32 delay_sum[SCB_RETRY_SHORT_DEF]; 
	uint32 delay_min;	
	uint32 delay_max;	
	uint32 delay_avg;	
	uint32 delay_hist[WLPKTDLY_HIST_NBINS];	
} scb_delay_stats_t;


typedef struct txdelay_event {
	uint8	status;
	int		rssi;
	chanim_stats_t		chanim_stats;
	scb_delay_stats_t	delay_stats[AC_COUNT];
} txdelay_event_t;


typedef struct txdelay_params {
	uint16	ratio;	
	uint8	cnt;	
	uint8	period;	
	uint8	tune;	
} txdelay_params_t;

enum {
	WNM_SERVICE_DMS = 1,
	WNM_SERVICE_FMS = 2,
	WNM_SERVICE_TFS = 3
};


typedef struct wl_tclas {
	uint8 user_priority;
	uint8 fc_len;
	dot11_tclas_fc_t fc;
} wl_tclas_t;

#define WL_TCLAS_FIXED_SIZE	OFFSETOF(wl_tclas_t, fc)

typedef struct wl_tclas_list {
	uint32 num;
	wl_tclas_t tclas[1];
} wl_tclas_list_t;


typedef struct wl_tfs_req {
	uint8 tfs_id;
	uint8 tfs_actcode;
	uint8 tfs_subelem_id;
	uint8 send;
} wl_tfs_req_t;

typedef struct wl_tfs_filter {
	uint8 status;			
	uint8 tclas_proc;		
	uint8 tclas_cnt;		
	uint8 tclas[1];			
} wl_tfs_filter_t;
#define WL_TFS_FILTER_FIXED_SIZE	OFFSETOF(wl_tfs_filter_t, tclas)

typedef struct wl_tfs_fset {
	struct ether_addr ea;		
	uint8 tfs_id;			
	uint8 status;			
	uint8 actcode;			
	uint8 token;			
	uint8 notify;			
	uint8 filter_cnt;		
	uint8 filter[1];		
} wl_tfs_fset_t;
#define WL_TFS_FSET_FIXED_SIZE		OFFSETOF(wl_tfs_fset_t, filter)

enum {
	TFS_STATUS_DISABLED = 0,	
	TFS_STATUS_DISABLING = 1,	
	TFS_STATUS_VALIDATED = 2,	
	TFS_STATUS_VALIDATING = 3,	
	TFS_STATUS_NOT_ASSOC = 4,	
	TFS_STATUS_NOT_SUPPORT = 5,	
	TFS_STATUS_DENIED = 6,		
};

typedef struct wl_tfs_status {
	uint8 fset_cnt;			
	wl_tfs_fset_t fset[1];		
} wl_tfs_status_t;

typedef struct wl_tfs_set {
	uint8 send;			
	uint8 tfs_id;			
	uint8 actcode;			
	uint8 tclas_proc;		
} wl_tfs_set_t;

typedef struct wl_tfs_term {
	uint8 del;			
	uint8 tfs_id;			
} wl_tfs_term_t;


#define DMS_DEP_PROXY_ARP (1 << 0)


enum {
	DMS_STATUS_DISABLED = 0,	
	DMS_STATUS_ACCEPTED = 1,	
	DMS_STATUS_NOT_ASSOC = 2,	
	DMS_STATUS_NOT_SUPPORT = 3,	
	DMS_STATUS_DENIED = 4,		
	DMS_STATUS_TERM = 5,		
	DMS_STATUS_REMOVING = 6,	
	DMS_STATUS_ADDING = 7,		
	DMS_STATUS_ERROR = 8,		
	DMS_STATUS_IN_PROGRESS = 9, 
	DMS_STATUS_REQ_MISMATCH = 10 
};

typedef struct wl_dms_desc {
	uint8 user_id;
	uint8 status;
	uint8 token;
	uint8 dms_id;
	uint8 tclas_proc;
	uint8 mac_len;		
	uint8 tclas_len;	
	uint8 data[1];		
} wl_dms_desc_t;

#define WL_DMS_DESC_FIXED_SIZE	OFFSETOF(wl_dms_desc_t, data)

typedef struct wl_dms_status {
	uint32 cnt;
	wl_dms_desc_t desc[1];
} wl_dms_status_t;

typedef struct wl_dms_set {
	uint8 send;
	uint8 user_id;
	uint8 tclas_proc;
} wl_dms_set_t;

typedef struct wl_dms_term {
	uint8 del;
	uint8 user_id;
} wl_dms_term_t;

typedef struct wl_service_term {
	uint8 service;
	union {
		wl_dms_term_t dms;
	} u;
} wl_service_term_t;


typedef struct wl_bsstrans_req {
	uint16 tbtt;			
	uint16 dur;			
	uint8 reqmode;			
	uint8 unicast;			
} wl_bsstrans_req_t;

enum {
	BSSTRANS_RESP_AUTO = 0,		
	BSSTRANS_RESP_DISABLE = 1,	
	BSSTRANS_RESP_ENABLE = 2,	
	BSSTRANS_RESP_WAIT = 3,		
	BSSTRANS_RESP_IMMEDIATE = 4	
};

typedef struct wl_bsstrans_resp {
	uint8 policy;
	uint8 status;
	uint8 delay;
	struct ether_addr target;
} wl_bsstrans_resp_t;


enum {
	WL_BSSTRANS_RESP_ROAM_ALWAYS = 0,	
	WL_BSSTRANS_RESP_ROAM_IF_MODE = 1,	
	WL_BSSTRANS_RESP_ROAM_IF_PREF = 2,	
	WL_BSSTRANS_RESP_WAIT = 3		
};


typedef struct wl_timbc_offset {
	int16 offset;		
	uint16 fix_intv;	
	uint16 rate_override;	
	uint8 tsf_present;	
} wl_timbc_offset_t;

typedef struct wl_timbc_set {
	uint8 interval;		
	uint8 flags;		
	uint16 rate_min;	
	uint16 rate_max;	
} wl_timbc_set_t;

enum {
	WL_TIMBC_SET_TSF_REQUIRED = 1,	
	WL_TIMBC_SET_NO_OVERRIDE = 2,	
	WL_TIMBC_SET_PROXY_ARP = 4,	
	WL_TIMBC_SET_DMS_ACCEPTED = 8	
};

typedef struct wl_timbc_status {
	uint8 status_sta;		
	uint8 status_ap;		
	uint8 interval;
	uint8 pad;
	int32 offset;
	uint16 rate_high;
	uint16 rate_low;
} wl_timbc_status_t;

enum {
	WL_TIMBC_STATUS_DISABLE = 0,		
	WL_TIMBC_STATUS_REQ_MISMATCH = 1,	
	WL_TIMBC_STATUS_NOT_ASSOC = 2,		
	WL_TIMBC_STATUS_NOT_SUPPORT = 3,	
	WL_TIMBC_STATUS_DENIED = 4,		
	WL_TIMBC_STATUS_ENABLE = 5		
};


typedef struct wl_pm2_sleep_ret_ext {
	uint8 logic;			
	uint16 low_ms;			
	uint16 high_ms;			
	uint16 rx_pkts_threshold;	
	uint16 tx_pkts_threshold;	
	uint16 txrx_pkts_threshold;	
	uint32 rx_bytes_threshold;	
	uint32 tx_bytes_threshold;	
	uint32 txrx_bytes_threshold;	
} wl_pm2_sleep_ret_ext_t;

#define WL_DFRTS_LOGIC_OFF	0	
#define WL_DFRTS_LOGIC_OR	1	
#define WL_DFRTS_LOGIC_AND	2	


#define WL_PASSACTCONV_DISABLE_NONE	0	
#define WL_PASSACTCONV_DISABLE_ALL	1	
#define WL_PASSACTCONV_DISABLE_PERM	2	


#define WL_RMC_CNT_VERSION	   1
#define WL_RMC_MAX_CLIENT	   32
#define WL_RMC_FLAG_INBLACKLIST	   1
#define WL_RMC_FLAG_ACTIVEACKER	   2
#define WL_RMC_FLAG_RELMCAST	   4
#define WL_RMC_MAX_TABLE_ENTRY     4

#define WL_RMC_VER		   1
#define WL_RMC_INDEX_ACK_ALL       255
#define WL_RMC_NUM_OF_MC_STREAMS   4
#define WL_RMC_MAX_TRS_PER_GROUP   1
#define WL_RMC_MAX_TRS_IN_ACKALL   1
#define WL_RMC_ACK_MCAST0          0x02
#define WL_RMC_ACK_MCAST_ALL       0x01
#define WL_RMC_ACTF_TIME_MIN       300	 
#define WL_RMC_ACTF_TIME_MAX       20000 
#define WL_RMC_ARTMO_MIN           350	 
#define WL_RMC_ARTMO_MAX           40000	 


enum rmc_opcodes {
	RELMCAST_ENTRY_OP_DISABLE = 0,   
	RELMCAST_ENTRY_OP_DELETE  = 1,   
	RELMCAST_ENTRY_OP_ENABLE  = 2,   
	RELMCAST_ENTRY_OP_ACK_ALL = 3    
};


enum rmc_modes {
	WL_RMC_MODE_RECEIVER    = 0,	 
	WL_RMC_MODE_TRANSMITTER = 1,	 
	WL_RMC_MODE_INITIATOR   = 2	 
};


typedef struct wl_relmcast_client {
	uint8 flag;			
	int16 rssi;			
	struct ether_addr addr;		
} wl_relmcast_client_t;


typedef struct wl_rmc_cnts {
	uint16  version;		
	uint16  length;			
	uint16	dupcnt;			
	uint16	ackreq_err;		
	uint16	af_tx_err;		
	uint16	null_tx_err;		
	uint16	af_unicast_tx_err;	
	uint16	mc_no_amt_slot;		
	uint16	mc_no_glb_slot;		
	uint16	mc_not_mirrored;	
	uint16	mc_existing_tr;		
	uint16	mc_exist_in_amt;	
	uint16	mc_not_exist_in_gbl;	
	uint16	mc_not_exist_in_amt;	
	uint16	mc_utilized;		
	uint16	mc_taken_other_tr;	
	uint32	rmc_rx_frames_mac;      
	uint32	rmc_tx_frames_mac;      
	uint32	mc_null_ar_cnt;         
	uint32	mc_ar_role_selected;	
	uint32	mc_ar_role_deleted;	
	uint32	mc_noacktimer_expired;  
} wl_rmc_cnts_t;


typedef struct wl_relmcast_st {
	uint8         ver;		
	uint8         num;		
	wl_relmcast_client_t clients[WL_RMC_MAX_CLIENT];
	uint16        err;		
	uint16        actf_time;	
} wl_relmcast_status_t;


typedef struct wl_rmc_entry {
	
	int8    flag;
	struct ether_addr addr;		
} wl_rmc_entry_t;


typedef struct wl_rmc_entry_table {
	uint8   index;			
	uint8   opcode;			
	wl_rmc_entry_t entry[WL_RMC_MAX_TABLE_ENTRY];
} wl_rmc_entry_table_t;


typedef struct wl_rmc_trans_info {
	struct ether_addr addr;		
	uint32 time_val;		
	uint16 seq;			
	uint16 artmo;
} wl_rmc_trans_info_t;


typedef struct wl_rmc_grp_entry {
	struct ether_addr   mcaddr;	
	struct ether_addr   ar;		
	wl_rmc_trans_info_t tr_info[WL_RMC_MAX_TRS_PER_GROUP];
} wl_rmc_grp_entry_t;


typedef struct wl_rmc_ackall_entry {
	struct ether_addr   ar;		
	wl_rmc_trans_info_t tr_info[WL_RMC_NUM_OF_MC_STREAMS];
} wl_rmc_ackall_entry_t;


typedef struct wl_rmc_gbl_table {
	uint8     activeMask;		
	wl_rmc_ackall_entry_t ackAll;   
	wl_rmc_grp_entry_t mc_entry[WL_RMC_NUM_OF_MC_STREAMS];
} wl_rmc_gbl_table_t;


typedef struct wl_rmc_vsie {
	uint8	oui[DOT11_OUI_LEN];
	uint16	payload;	
} wl_rmc_vsie_t;

typedef struct wl_proxd_iovar {
	uint16	method;		
	uint16	mode;		
} wl_proxd_iovar_t;


typedef struct wl_proxd_params_rssi_method {
	chanspec_t	chanspec;	
	uint16		interval;	
	uint16		duration;	
	int16		rssi_thresh;	
	int16		tx_power;	
	uint16		tx_rate;	
	uint16		timeout;	
	uint16		maxconvergtmo;	
} wl_proxd_params_rssi_method_t;

typedef struct wl_proxd_params_iovar {
	uint16	method;			
	union {
		wl_proxd_params_rssi_method_t rssi_params;
	} u;				
} wl_proxd_params_iovar_t;

enum {
	RSSI_REASON_UNKNOW,
	RSSI_REASON_LOWRSSI,
	RSSI_REASON_NSYC,
	RSSI_REASON_TIMEOUT
};

enum {
	RSSI_STATE_POLL,
	RSSI_STATE_TPAIRING,
	RSSI_STATE_IPAIRING,
	RSSI_STATE_THANDSHAKE,
	RSSI_STATE_IHANDSHAKE,
	RSSI_STATE_CONFIRMED,
	RSSI_STATE_PIPELINE,
	RSSI_STATE_NEGMODE,
	RSSI_STATE_MONITOR,
	RSSI_STATE_LAST
};

typedef struct wl_proxd_status_iovar {
	uint8			mode;
	uint8			peermode;
	uint8			state;
	uint8			reason;
	uint32			txcnt;
	uint32			rxcnt;
	struct ether_addr	peer;
	int16			hi_rssi;
	int16			low_rssi;
} wl_proxd_status_iovar_t;

#ifdef NET_DETECT
typedef struct net_detect_adapter_features {
	bool	wowl_enabled;
	bool	net_detect_enabled;
	bool	nlo_enabled;
} net_detect_adapter_features_t;

typedef enum net_detect_bss_type {
	nd_bss_any = 0,
	nd_ibss,
	nd_ess
} net_detect_bss_type_t;

typedef struct net_detect_profile {
	wlc_ssid_t		ssid;
	net_detect_bss_type_t   bss_type;	
	uint32			cipher_type;	
	uint32			auth_type;	
} net_detect_profile_t;

typedef struct net_detect_profile_list {
	uint32			num_nd_profiles;
	net_detect_profile_t	nd_profile[0];
} net_detect_profile_list_t;

typedef struct net_detect_config {
	bool			    nd_enabled;
	uint32			    scan_interval;
	uint32			    wait_period;
	bool			    wake_if_connected;
	bool			    wake_if_disconnected;
	net_detect_profile_list_t   nd_profile_list;
} net_detect_config_t;

typedef enum net_detect_wake_reason {
	nd_reason_unknown,
	nd_net_detected,
	nd_wowl_event,
	nd_ucode_error
} net_detect_wake_reason_t;

typedef struct net_detect_wake_data {
	net_detect_wake_reason_t    nd_wake_reason;
	uint32			    nd_wake_date_length;
	uint8			    nd_wake_data[0];	    
} net_detect_wake_data_t;

#endif 

#endif 

typedef struct bcnreq {
	uint8 bcn_mode;
	int dur;
	int channel;
	struct ether_addr da;
	uint16 random_int;
	wlc_ssid_t ssid;
	uint16 reps;
} bcnreq_t;

typedef struct rrmreq {
	struct ether_addr da;
	uint8 reg;
	uint8 chan;
	uint16 random_int;
	uint16 dur;
	uint16 reps;
} rrmreq_t;

typedef struct framereq {
	struct ether_addr da;
	uint8 reg;
	uint8 chan;
	uint16 random_int;
	uint16 dur;
	struct ether_addr ta;
	uint16 reps;
} framereq_t;

typedef struct statreq {
	struct ether_addr da;
	struct ether_addr peer;
	uint16 random_int;
	uint16 dur;
	uint8 group_id;
	uint16 reps;
} statreq_t;

typedef struct wlc_l2keepalive_ol_params {
	uint8	flags;
	uint8	prio;
	uint16	period_ms;
} wlc_l2keepalive_ol_params_t;

typedef struct wlc_dwds_config {
	uint32		enable;
	uint32		mode; 
	struct ether_addr ea;
} wlc_dwds_config_t;

typedef struct wl_el_set_params_s {
	uint8 set;	
	uint32 size;	
} wl_el_set_params_t;

typedef struct wl_el_tag_params_s {
	uint16 tag;
	uint8 set;
	uint8 flags;
} wl_el_tag_params_t;


#define INTFER_VERSION		1
typedef struct wl_intfer_params {
	uint16 version;			
	uint8 period;			
	uint8 cnt;			
	uint8 txfail_thresh;	
	uint8 tcptxfail_thresh;	
} wl_intfer_params_t;

typedef struct wl_staprio_cfg {
	struct ether_addr ea;	
	uint8 prio;		
} wl_staprio_cfg_t;

typedef enum wl_stamon_cfg_cmd_type {
	STAMON_CFG_CMD_DEL = 0,
	STAMON_CFG_CMD_ADD = 1
} wl_stamon_cfg_cmd_type_t;

typedef struct wlc_stamon_sta_config {
	wl_stamon_cfg_cmd_type_t cmd; 
	struct ether_addr ea;
} wlc_stamon_sta_config_t;


#define WL_LAST_BCNS_INFO_FIXED_LEN		OFFSETOF(wlc_bcn_len_hist_t, bcnlen_ring)
typedef struct wlc_bcn_len_hist {
	uint16	ver;				
	uint16	cur_index;			
	uint32	max_bcnlen;		
	uint32	min_bcnlen;		
	uint32	ringbuff_len;		
	uint32	bcnlen_ring[1];	
} wlc_bcn_len_hist_t;


#define WL_WDSIFTYPE_NONE  0x0 
#define WL_WDSIFTYPE_WDS   0x1 
#define WL_WDSIFTYPE_DWDS  0x2 

typedef struct wl_bssload_static {
	bool is_static;
	uint16 sta_count;
	uint8 chan_util;
	uint16 aac;
} wl_bssload_static_t;

#ifdef ATE_BUILD

typedef enum wl_gpaio_option {
	GPAIO_PMU_AFELDO,
	GPAIO_PMU_TXLDO,
	GPAIO_PMU_VCOLDO,
	GPAIO_PMU_LNALDO,
	GPAIO_PMU_ADCLDO,
	GPAIO_PMU_CLEAR
} wl_gpaio_option_t;
#endif 

#endif 

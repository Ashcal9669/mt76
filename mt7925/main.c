// SPDX-License-Identifier: BSD-3-Clause-Clear
/* Copyright (C) 2023 MediaTek Inc. */

#include <linux/etherdevice.h>
#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <net/ipv6.h>
#include "mt7925.h"
#include "mlo/mlo.h"
#include "regd.h"
#include "mcu.h"
#include "mac.h"

static void
mt7925_init_he_caps(struct mt792x_phy *phy, enum nl80211_band band,
		    struct ieee80211_sband_iftype_data *data,
		    enum nl80211_iftype iftype)
{
	struct ieee80211_sta_he_cap *he_cap = &data->he_cap;
	struct ieee80211_he_cap_elem *he_cap_elem = &he_cap->he_cap_elem;
	struct ieee80211_he_mcs_nss_supp *he_mcs = &he_cap->he_mcs_nss_supp;
	int i, nss = hweight8(phy->mt76->antenna_mask);
	u16 mcs_map = 0;

	for (i = 0; i < 8; i++) {
		if (i < nss)
			mcs_map |= (IEEE80211_HE_MCS_SUPPORT_0_11 << (i * 2));
		else
			mcs_map |= (IEEE80211_HE_MCS_NOT_SUPPORTED << (i * 2));
	}

	he_cap->has_he = true;

	he_cap_elem->mac_cap_info[0] = IEEE80211_HE_MAC_CAP0_HTC_HE;
	he_cap_elem->mac_cap_info[3] = IEEE80211_HE_MAC_CAP3_OMI_CONTROL |
				       IEEE80211_HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_EXT_3;
	he_cap_elem->mac_cap_info[4] = IEEE80211_HE_MAC_CAP4_AMSDU_IN_AMPDU;

	if (band == NL80211_BAND_2GHZ)
		he_cap_elem->phy_cap_info[0] =
			IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_IN_2G;
	else
		he_cap_elem->phy_cap_info[0] =
			IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G |
			IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G;

	he_cap_elem->phy_cap_info[1] =
		IEEE80211_HE_PHY_CAP1_LDPC_CODING_IN_PAYLOAD;
	he_cap_elem->phy_cap_info[2] =
		IEEE80211_HE_PHY_CAP2_NDP_4x_LTF_AND_3_2US |
		IEEE80211_HE_PHY_CAP2_STBC_TX_UNDER_80MHZ |
		IEEE80211_HE_PHY_CAP2_STBC_RX_UNDER_80MHZ |
		IEEE80211_HE_PHY_CAP2_UL_MU_FULL_MU_MIMO |
		IEEE80211_HE_PHY_CAP2_UL_MU_PARTIAL_MU_MIMO;

	switch (iftype) {
	case NL80211_IFTYPE_AP:
		he_cap_elem->mac_cap_info[2] |=
			IEEE80211_HE_MAC_CAP2_BSR;
		he_cap_elem->mac_cap_info[4] |=
			IEEE80211_HE_MAC_CAP4_BQR;
		he_cap_elem->mac_cap_info[5] |=
			IEEE80211_HE_MAC_CAP5_OM_CTRL_UL_MU_DATA_DIS_RX;
		he_cap_elem->phy_cap_info[3] |=
			IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_TX_QPSK |
			IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_RX_QPSK;
		he_cap_elem->phy_cap_info[6] |=
			IEEE80211_HE_PHY_CAP6_PARTIAL_BW_EXT_RANGE |
			IEEE80211_HE_PHY_CAP6_PPE_THRESHOLD_PRESENT;
		he_cap_elem->phy_cap_info[9] |=
			IEEE80211_HE_PHY_CAP9_TX_1024_QAM_LESS_THAN_242_TONE_RU |
			IEEE80211_HE_PHY_CAP9_RX_1024_QAM_LESS_THAN_242_TONE_RU;
		break;
	case NL80211_IFTYPE_STATION:
		he_cap_elem->mac_cap_info[1] |=
			IEEE80211_HE_MAC_CAP1_TF_MAC_PAD_DUR_16US;

		if (band == NL80211_BAND_2GHZ)
			he_cap_elem->phy_cap_info[0] |=
				IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_RU_MAPPING_IN_2G;
		else
			he_cap_elem->phy_cap_info[0] |=
				IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_RU_MAPPING_IN_5G;

		he_cap_elem->phy_cap_info[1] |=
			IEEE80211_HE_PHY_CAP1_DEVICE_CLASS_A |
			IEEE80211_HE_PHY_CAP1_HE_LTF_AND_GI_FOR_HE_PPDUS_0_8US;
		he_cap_elem->phy_cap_info[3] |=
			IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_TX_QPSK |
			IEEE80211_HE_PHY_CAP3_DCM_MAX_CONST_RX_QPSK;
		he_cap_elem->phy_cap_info[4] |=
			IEEE80211_HE_PHY_CAP4_SU_BEAMFORMEE |
			IEEE80211_HE_PHY_CAP4_BEAMFORMEE_MAX_STS_UNDER_80MHZ_4 |
			IEEE80211_HE_PHY_CAP4_BEAMFORMEE_MAX_STS_ABOVE_80MHZ_4;
		he_cap_elem->phy_cap_info[5] |=
			IEEE80211_HE_PHY_CAP5_NG16_SU_FEEDBACK |
			IEEE80211_HE_PHY_CAP5_NG16_MU_FEEDBACK;
		he_cap_elem->phy_cap_info[6] |=
			IEEE80211_HE_PHY_CAP6_CODEBOOK_SIZE_42_SU |
			IEEE80211_HE_PHY_CAP6_CODEBOOK_SIZE_75_MU |
			IEEE80211_HE_PHY_CAP6_TRIG_CQI_FB |
			IEEE80211_HE_PHY_CAP6_PARTIAL_BW_EXT_RANGE |
			IEEE80211_HE_PHY_CAP6_PPE_THRESHOLD_PRESENT;
		he_cap_elem->phy_cap_info[7] |=
			IEEE80211_HE_PHY_CAP7_POWER_BOOST_FACTOR_SUPP |
			IEEE80211_HE_PHY_CAP7_HE_SU_MU_PPDU_4XLTF_AND_08_US_GI;
		he_cap_elem->phy_cap_info[8] |=
			IEEE80211_HE_PHY_CAP8_20MHZ_IN_40MHZ_HE_PPDU_IN_2G |
			IEEE80211_HE_PHY_CAP8_20MHZ_IN_160MHZ_HE_PPDU |
			IEEE80211_HE_PHY_CAP8_80MHZ_IN_160MHZ_HE_PPDU |
			IEEE80211_HE_PHY_CAP8_DCM_MAX_RU_484;
		he_cap_elem->phy_cap_info[9] |=
			IEEE80211_HE_PHY_CAP9_LONGER_THAN_16_SIGB_OFDM_SYM |
			IEEE80211_HE_PHY_CAP9_NON_TRIGGERED_CQI_FEEDBACK |
			IEEE80211_HE_PHY_CAP9_TX_1024_QAM_LESS_THAN_242_TONE_RU |
			IEEE80211_HE_PHY_CAP9_RX_1024_QAM_LESS_THAN_242_TONE_RU |
			IEEE80211_HE_PHY_CAP9_RX_FULL_BW_SU_USING_MU_WITH_COMP_SIGB |
			IEEE80211_HE_PHY_CAP9_RX_FULL_BW_SU_USING_MU_WITH_NON_COMP_SIGB;
		break;
	default:
		break;
	}

	he_mcs->rx_mcs_80 = cpu_to_le16(mcs_map);
	he_mcs->tx_mcs_80 = cpu_to_le16(mcs_map);
	he_mcs->rx_mcs_160 = cpu_to_le16(mcs_map);
	he_mcs->tx_mcs_160 = cpu_to_le16(mcs_map);

	memset(he_cap->ppe_thres, 0, sizeof(he_cap->ppe_thres));

	if (he_cap_elem->phy_cap_info[6] &
	    IEEE80211_HE_PHY_CAP6_PPE_THRESHOLD_PRESENT) {
		mt76_connac_gen_ppe_thresh(he_cap->ppe_thres, nss, band);
	} else {
		he_cap_elem->phy_cap_info[9] |=
			u8_encode_bits(IEEE80211_HE_PHY_CAP9_NOMINAL_PKT_PADDING_16US,
				       IEEE80211_HE_PHY_CAP9_NOMINAL_PKT_PADDING_MASK);
	}

	if (band == NL80211_BAND_6GHZ) {
		struct ieee80211_supported_band *sband =
			&phy->mt76->sband_5g.sband;
		struct ieee80211_sta_ht_cap *ht_cap = &sband->ht_cap;

		u16 cap = IEEE80211_HE_6GHZ_CAP_TX_ANTPAT_CONS |
			  IEEE80211_HE_6GHZ_CAP_RX_ANTPAT_CONS;

		cap |= u16_encode_bits(ht_cap->ampdu_density,
				       IEEE80211_HE_6GHZ_CAP_MIN_MPDU_START) |
		       u16_encode_bits(IEEE80211_VHT_MAX_AMPDU_1024K,
				       IEEE80211_HE_6GHZ_CAP_MAX_AMPDU_LEN_EXP) |
		       u16_encode_bits(IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_11454,
				       IEEE80211_HE_6GHZ_CAP_MAX_MPDU_LEN);

		data->he_6ghz_capa.capa = cpu_to_le16(cap);
	}
}

static void
mt7925_init_eht_caps(struct mt792x_phy *phy, enum nl80211_band band,
		     struct ieee80211_sband_iftype_data *data)
{
	struct ieee80211_sta_eht_cap *eht_cap = &data->eht_cap;
	struct ieee80211_eht_cap_elem_fixed *eht_cap_elem = &eht_cap->eht_cap_elem;
	struct ieee80211_eht_mcs_nss_supp *eht_nss = &eht_cap->eht_mcs_nss_supp;
	enum nl80211_chan_width width = phy->mt76->chandef.width;
	int nss = hweight8(phy->mt76->antenna_mask);
	int sts = hweight16(phy->mt76->chainmask);
	u8 val;

	if (!phy->dev->has_eht)
		return;

	eht_cap->has_eht = true;

	eht_cap_elem->mac_cap_info[0] =
		IEEE80211_EHT_MAC_CAP0_EPCS_PRIO_ACCESS |
		IEEE80211_EHT_MAC_CAP0_OM_CONTROL;

	eht_cap_elem->phy_cap_info[0] =
		IEEE80211_EHT_PHY_CAP0_NDP_4_EHT_LFT_32_GI |
		IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMER |
		IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMEE;

	if (band == NL80211_BAND_6GHZ && is_320mhz_supported(&phy->dev->mt76))
		eht_cap_elem->phy_cap_info[0] |=
			IEEE80211_EHT_PHY_CAP0_320MHZ_IN_6GHZ;

	eht_cap_elem->phy_cap_info[0] |=
		u8_encode_bits(u8_get_bits(sts - 1, BIT(0)),
			       IEEE80211_EHT_PHY_CAP0_BEAMFORMEE_SS_80MHZ_MASK);

	eht_cap_elem->phy_cap_info[1] =
		u8_encode_bits(u8_get_bits(sts - 1, GENMASK(2, 1)),
			       IEEE80211_EHT_PHY_CAP1_BEAMFORMEE_SS_80MHZ_MASK) |
		u8_encode_bits(sts - 1,
			       IEEE80211_EHT_PHY_CAP1_BEAMFORMEE_SS_160MHZ_MASK);

	if (band == NL80211_BAND_6GHZ && is_320mhz_supported(&phy->dev->mt76))
		eht_cap_elem->phy_cap_info[1] |=
			u8_encode_bits(sts - 1,
				       IEEE80211_EHT_PHY_CAP1_BEAMFORMEE_SS_320MHZ_MASK);

	eht_cap_elem->phy_cap_info[2] =
		u8_encode_bits(sts - 1, IEEE80211_EHT_PHY_CAP2_SOUNDING_DIM_80MHZ_MASK) |
		u8_encode_bits(sts - 1, IEEE80211_EHT_PHY_CAP2_SOUNDING_DIM_160MHZ_MASK);

	if (band == NL80211_BAND_6GHZ && is_320mhz_supported(&phy->dev->mt76))
		eht_cap_elem->phy_cap_info[2] |=
			u8_encode_bits(sts - 1,
				       IEEE80211_EHT_PHY_CAP2_SOUNDING_DIM_320MHZ_MASK);

	eht_cap_elem->phy_cap_info[3] =
		IEEE80211_EHT_PHY_CAP3_NG_16_SU_FEEDBACK |
		IEEE80211_EHT_PHY_CAP3_NG_16_MU_FEEDBACK |
		IEEE80211_EHT_PHY_CAP3_CODEBOOK_4_2_SU_FDBK |
		IEEE80211_EHT_PHY_CAP3_CODEBOOK_7_5_MU_FDBK |
		IEEE80211_EHT_PHY_CAP3_TRIG_SU_BF_FDBK |
		IEEE80211_EHT_PHY_CAP3_TRIG_MU_BF_PART_BW_FDBK |
		IEEE80211_EHT_PHY_CAP3_TRIG_CQI_FDBK;

	eht_cap_elem->phy_cap_info[4] =
		u8_encode_bits(min_t(int, sts - 1, 2),
			       IEEE80211_EHT_PHY_CAP4_MAX_NC_MASK);

	eht_cap_elem->phy_cap_info[5] =
		IEEE80211_EHT_PHY_CAP5_NON_TRIG_CQI_FEEDBACK |
		u8_encode_bits(IEEE80211_EHT_PHY_CAP5_COMMON_NOMINAL_PKT_PAD_16US,
			       IEEE80211_EHT_PHY_CAP5_COMMON_NOMINAL_PKT_PAD_MASK) |
		u8_encode_bits(u8_get_bits(0x11, GENMASK(1, 0)),
			       IEEE80211_EHT_PHY_CAP5_MAX_NUM_SUPP_EHT_LTF_MASK);

	val = width == NL80211_CHAN_WIDTH_320 ? 0xf :
	      width == NL80211_CHAN_WIDTH_160 ? 0x7 :
	      width == NL80211_CHAN_WIDTH_80 ? 0x3 : 0x1;
	eht_cap_elem->phy_cap_info[6] =
		u8_encode_bits(u8_get_bits(0x11, GENMASK(4, 2)),
			       IEEE80211_EHT_PHY_CAP6_MAX_NUM_SUPP_EHT_LTF_MASK) |
		u8_encode_bits(val, IEEE80211_EHT_PHY_CAP6_MCS15_SUPP_MASK);

	eht_cap_elem->phy_cap_info[7] =
		IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_80MHZ |
		IEEE80211_EHT_PHY_CAP7_NON_OFDMA_UL_MU_MIMO_160MHZ |
		IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_80MHZ |
		IEEE80211_EHT_PHY_CAP7_MU_BEAMFORMER_160MHZ;

	val = u8_encode_bits(nss, IEEE80211_EHT_MCS_NSS_RX) |
	      u8_encode_bits(nss, IEEE80211_EHT_MCS_NSS_TX);

	eht_nss->bw._80.rx_tx_mcs9_max_nss = val;
	eht_nss->bw._80.rx_tx_mcs11_max_nss = val;
	eht_nss->bw._80.rx_tx_mcs13_max_nss = val;
	eht_nss->bw._160.rx_tx_mcs9_max_nss = val;
	eht_nss->bw._160.rx_tx_mcs11_max_nss = val;
	eht_nss->bw._160.rx_tx_mcs13_max_nss = val;
	if (band == NL80211_BAND_6GHZ && is_320mhz_supported(&phy->dev->mt76)) {
		eht_nss->bw._320.rx_tx_mcs9_max_nss = val;
		eht_nss->bw._320.rx_tx_mcs11_max_nss = val;
		eht_nss->bw._320.rx_tx_mcs13_max_nss = val;
	}
}

int mt7925_init_mlo_caps(struct mt792x_phy *phy)
{
	struct wiphy *wiphy = phy->mt76->hw->wiphy;
	struct wiphy_iftype_ext_capab *ext_capab;
	u8 simul_links;
	static const u8 ext_capa_sta[] = {
		[0] = WLAN_EXT_CAPA1_EXT_CHANNEL_SWITCHING,
		[2] = WLAN_EXT_CAPA3_MULTI_BSSID_SUPPORT,
		[7] = WLAN_EXT_CAPA8_OPMODE_NOTIF,
	};
	static const struct wiphy_iftype_ext_capab ext_capab_template[] = {
		{
			.iftype = NL80211_IFTYPE_STATION,
			.extended_capabilities = ext_capa_sta,
			.extended_capabilities_mask = ext_capa_sta,
			.extended_capabilities_len = sizeof(ext_capa_sta),
		},
	};

	if (!(phy->chip_cap & MT792x_CHIP_CAP_MLO_EN))
		return 0;

	ext_capab = devm_kmemdup(phy->dev->mt76.dev, ext_capab_template,
				 sizeof(ext_capab_template), GFP_KERNEL);
	if (!ext_capab)
		return -ENOMEM;

	ext_capab[0].eml_capabilities = phy->eml_cap;
	/* IEEE 802.11be encodes "Maximum Number of Simultaneous Links" as
	 * (N - 1), so 0 advertises a single simultaneous link and 1
	 * advertises two. MT7927 is dual-band-concurrent (DBDC is enabled at
	 * init and the vendor Windows driver runs two links simultaneously on
	 * this silicon), so 1 is the value the hardware should be able to
	 * honour.
	 *
	 * Runtime-selectable so the behaviour can be A/B tested without a
	 * rebuild. Default stays at the upstream-safe 0 until the two-link
	 * path is proven.
	 */
	simul_links = mt7925_mlo_max_simul_links;
	if (simul_links == MT7925_MLO_SIMUL_LINKS_AUTO) {
		/* MT7927 is dual-band concurrent: DBDC is enabled at init and
		 * the firmware grants a multi-link (JOIN) ROC covering both
		 * bands, so it can genuinely hold two links at once. Every
		 * other mt7925-class part is single-link (MLSR) only.
		 */
		simul_links = is_mt7927(&phy->dev->mt76) ? 1 : 0;
	}

	ext_capab[0].mld_capa_and_ops =
		u16_encode_bits(simul_links & 0xf,
				IEEE80211_MLD_CAP_OP_MAX_SIMUL_LINKS);
	mt7925_mlo_update_caps(phy, &ext_capab[0].mld_capa_and_ops);

	wiphy->flags |= WIPHY_FLAG_SUPPORTS_MLO;
	wiphy->iftype_ext_capab = ext_capab;
	wiphy->num_iftype_ext_capab = ARRAY_SIZE(ext_capab_template);

	return 0;
}

static void
__mt7925_set_stream_he_eht_caps(struct mt792x_phy *phy,
				struct ieee80211_supported_band *sband,
				enum nl80211_band band)
{
	struct ieee80211_sband_iftype_data *data = phy->iftype[band];
	int i, n = 0;

	for (i = 0; i < NUM_NL80211_IFTYPES; i++) {
		switch (i) {
		case NL80211_IFTYPE_STATION:
		case NL80211_IFTYPE_AP:
			break;
		default:
			continue;
		}

		data[n].types_mask = BIT(i);
		mt7925_init_he_caps(phy, band, &data[n], i);
		mt7925_init_eht_caps(phy, band, &data[n]);

		n++;
	}

	_ieee80211_set_sband_iftype_data(sband, data, n);
}

void mt7925_set_stream_he_eht_caps(struct mt792x_phy *phy)
{
	if (phy->mt76->cap.has_2ghz)
		__mt7925_set_stream_he_eht_caps(phy, &phy->mt76->sband_2g.sband,
						NL80211_BAND_2GHZ);

	if (phy->mt76->cap.has_5ghz)
		__mt7925_set_stream_he_eht_caps(phy, &phy->mt76->sband_5g.sband,
						NL80211_BAND_5GHZ);

	if (phy->mt76->cap.has_6ghz)
		__mt7925_set_stream_he_eht_caps(phy, &phy->mt76->sband_6g.sband,
						NL80211_BAND_6GHZ);
}

int __mt7925_start(struct mt792x_phy *phy)
{
	struct mt76_phy *mphy = phy->mt76;
	int err;

	err = mt7925_mcu_set_channel_domain(mphy);
	if (err)
		return err;

	err = mt7925_mcu_set_rts_thresh(phy, 0x92b);
	if (err)
		return err;

	mt792x_mac_reset_counters(phy);
	set_bit(MT76_STATE_RUNNING, &mphy->state);

	ieee80211_queue_delayed_work(mphy->hw, &mphy->mac_work,
				     MT792x_WATCHDOG_TIME);

	if (phy->chip_cap & MT792x_CHIP_CAP_WF_RF_PIN_CTRL_EVT_EN)
		wiphy_rfkill_start_polling(mphy->hw->wiphy);

	return 0;
}
EXPORT_SYMBOL_GPL(__mt7925_start);

static int mt7925_start(struct ieee80211_hw *hw)
{
	struct mt792x_phy *phy = mt792x_hw_phy(hw);
	int err;

	mt792x_mutex_acquire(phy->dev);
	err = __mt7925_start(phy);
	mt792x_mutex_release(phy->dev);

	return err;
}

static int mt7925_mac_link_bss_add(struct mt792x_dev *dev,
				   struct ieee80211_bss_conf *link_conf,
				   struct mt792x_link_sta *mlink)
{
	struct mt792x_bss_conf *mconf = mt792x_link_conf_to_mconf(link_conf);
	struct ieee80211_vif *vif = link_conf->vif;
	struct mt792x_vif *mvif = mconf->vif;
	struct mt76_txq *mtxq;
	int idx, ret = 0;

	if (vif->type == NL80211_IFTYPE_P2P_DEVICE) {
		mconf->mt76.idx = MT792x_MAX_INTERFACES;
	} else {
		mconf->mt76.idx = __ffs64(~dev->mt76.vif_mask);

		if (mconf->mt76.idx >= MT792x_MAX_INTERFACES) {
			ret = -ENOSPC;
			goto out;
		}
	}

	mconf->mt76.omac_idx = ieee80211_vif_is_mld(vif) ?
			       0 : mconf->mt76.idx;
	mconf->mt76.band_idx = 0xff;

	if (is_mt7927(&dev->mt76)) {
		struct ieee80211_channel *chan;

		if (link_conf->chanreq.oper.chan)
			chan = link_conf->chanreq.oper.chan;
		else
			chan = mvif->phy->mt76->chandef.chan;

		mconf->mt76.band_idx = mt7927_band_idx(chan->band);
	}

	mconf->mt76.wmm_idx = ieee80211_vif_is_mld(vif) ?
			      0 : mconf->mt76.idx % MT76_CONNAC_MAX_WMM_SETS;
	mconf->mt76.link_idx = hweight16(mvif->valid_links);

	if (mvif->phy->mt76->chandef.chan->band != NL80211_BAND_2GHZ)
		mconf->mt76.basic_rates_idx = MT792x_BASIC_RATES_TBL + 4;
	else
		mconf->mt76.basic_rates_idx = MT792x_BASIC_RATES_TBL;

	dev->mt76.vif_mask |= BIT_ULL(mconf->mt76.idx);
	mvif->phy->omac_mask |= BIT_ULL(mconf->mt76.omac_idx);

	idx = MT792x_WTBL_RESERVED - mconf->mt76.idx;

	mlink->wcid.idx = idx;
	mlink->wcid.tx_info |= MT_WCID_TX_INFO_SET;
	mt76_wcid_init(&mlink->wcid, 0);

	mt7925_mac_wtbl_update(dev, idx,
			       MT_WTBL_UPDATE_ADM_COUNT_CLEAR);

	ewma_rssi_init(&mconf->rssi);

	rcu_assign_pointer(dev->mt76.wcid[idx], &mlink->wcid);

	if (READ_ONCE(mt76_mlo_diag_trace))
		printk(KERN_INFO
		      "MLO_BSS_ADD_REACHED: link_id=%u is_primary=%d bss_idx=%u wcid=%u\n",
		      mconf->link_id, mconf == &mvif->bss_conf,
		      mconf->mt76.idx, mlink->wcid.idx);

	ret = mt76_connac_mcu_uni_add_dev(&dev->mphy, link_conf, &mconf->mt76,
					  &mlink->wcid, true);
	if (ret)
		goto free_host;

	if (vif->txq) {
		mtxq = (struct mt76_txq *)vif->txq->drv_priv;
		mtxq->wcid = idx;
	}

	return 0;

free_host:
	/* The firmware BSS was never created, so unwind only the host-side
	 * state allocated above. Do NOT call mt792x_mac_link_bss_remove()
	 * here: it would issue a UNI_ADD_DEV(false) for a device the firmware
	 * does not know about.
	 */
	rcu_assign_pointer(dev->mt76.wcid[idx], NULL);

	dev->mt76.vif_mask &= ~BIT_ULL(mconf->mt76.idx);
	mvif->phy->omac_mask &= ~BIT_ULL(mconf->mt76.omac_idx);

	spin_lock_bh(&dev->mt76.sta_poll_lock);
	if (!list_empty(&mlink->wcid.poll_list))
		list_del_init(&mlink->wcid.poll_list);
	spin_unlock_bh(&dev->mt76.sta_poll_lock);

	mt76_wcid_cleanup(&dev->mt76, &mlink->wcid);

out:
	return ret;
}

static int
mt7925_add_interface(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	struct mt792x_phy *phy = mt792x_hw_phy(hw);
	int ret = 0;

	mt792x_mutex_acquire(dev);

	mvif->phy = phy;
	mvif->bss_conf.vif = mvif;
	mvif->sta.vif = mvif;
	mvif->deflink_id = IEEE80211_LINK_UNSPECIFIED;
	mvif->mlo_pm_state = MT792x_MLO_LINK_DISASSOC;

	ret = mt7925_mac_link_bss_add(dev, &vif->bss_conf, &mvif->sta.deflink);
	if (ret < 0)
		goto out;

	/* A monitor vif must stay a passive sniffer: never enable beacon
	 * filtering on it, or the firmware drops other-BSS beacons.
	 */
	if (vif->type != NL80211_IFTYPE_MONITOR)
		vif->driver_flags |= IEEE80211_VIF_BEACON_FILTER;
	if (phy->chip_cap & MT792x_CHIP_CAP_RSSI_NOTIFY_EVT_EN)
		vif->driver_flags |= IEEE80211_VIF_SUPPORTS_CQM_RSSI;

	INIT_WORK(&mvif->csa_work, mt7925_csa_work);
	timer_setup(&mvif->csa_timer, mt792x_csa_timer, 0);

	memset(mvif->mlo_link_rssi, MT792X_MLO_RSSI_UNKNOWN, sizeof(mvif->mlo_link_rssi));
	INIT_DELAYED_WORK(&mvif->mlo_steer_work, mt7925_mlo_steer_work);
	if (vif->type == NL80211_IFTYPE_STATION)
		ieee80211_queue_delayed_work(hw, &mvif->mlo_steer_work,
					     MT7925_MLO_STEER_INTERVAL);

out:
	mt792x_mutex_release(dev);

	return ret;
}

static void
mt7925_remove_interface(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct mt792x_dev *dev = mt792x_hw_dev(hw);

	cancel_delayed_work_sync(&mvif->mlo_steer_work);
	mvif->mlo_auto_steering = false;
	mt7925_mlo_update_rssi_monitor(dev, vif);
	mt792x_remove_interface(hw, vif);
}

/* MLO active-link steering: picks the best 2-link pair from measured
 * per-link RSSI and switches to it via ieee80211_set_active_links_async(),
 * reusing the existing STA_REC_MLD/firmware-update path -- no new MCU
 * command. See the mlo_link_rssi comment in mt792x.h for the real
 * limitation on which links can have fresh RSSI at any given time.
 */
#define MT7925_MLO_STEER_HYSTERESIS_DB	5
#define MT7925_MLO_STEER_COOLDOWN	(10 * HZ)
#define MT7925_MLO_RSSI_STALE_JIFFIES	(30 * HZ)

/* Band-pair preference bonus: deliberately smaller than a real, common RSSI
 * gap (tens of dB) so it only breaks near-ties between comparably-usable
 * pairs -- it must never make steering prefer a 5GHz link that is actually
 * weak just because it is 5GHz. RSSI stays the primary signal (added into
 * the same score before this bonus, mt7925_mlo_pair_score() below).
 */
#define MT7925_MLO_STEER_5G6G_BONUS	8

static const u16 mt7925_mlo_steer_pairs[] = { 0x3, 0x5, 0x6 };

static enum nl80211_band mt7925_mlo_link_band(struct ieee80211_vif *vif, int link_id)
{
	struct ieee80211_bss_conf *bss_conf;
	struct ieee80211_channel *chan;

	bss_conf = mt792x_vif_to_bss_conf(vif, link_id);
	if (!bss_conf)
		return NUM_NL80211_BANDS;

	chan = bss_conf->chanreq.oper.chan;
	if (!chan)
		return NUM_NL80211_BANDS;

	return chan->band;
}

static s32 mt7925_mlo_pair_bonus(struct ieee80211_vif *vif, u16 pair)
{
	unsigned long links = pair;
	bool has_5g = false, has_6g = false;
	int link_id;

	for_each_set_bit(link_id, &links, IEEE80211_MLD_MAX_NUM_LINKS) {
		switch (mt7925_mlo_link_band(vif, link_id)) {
		case NL80211_BAND_5GHZ:
			has_5g = true;
			break;
		case NL80211_BAND_6GHZ:
			has_6g = true;
			break;
		default:
			break;
		}
	}

	return (has_5g && has_6g) ? MT7925_MLO_STEER_5G6G_BONUS : 0;
}

/* Returns the RSSI-only score in *rssi_score and the band-pair bonus in
 * *bonus (both S32_MIN if the pair has no usable candidate data), so the
 * caller can trace them separately; the return value is their sum, the
 * actual comparison score.
 */
static s32 mt7925_mlo_pair_score_parts(struct ieee80211_vif *vif, u16 pair,
				       s32 *rssi_score, s32 *bonus)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	unsigned long links = pair;
	s32 score = 0;
	int link_id;

	for_each_set_bit(link_id, &links, IEEE80211_MLD_MAX_NUM_LINKS) {
		if (!(mvif->valid_links & BIT(link_id)))
			goto no_data;

		if (mvif->mlo_link_rssi[link_id] == MT792X_MLO_RSSI_UNKNOWN)
			goto no_data;

		if (time_after(jiffies, mvif->mlo_link_rssi_jiffies[link_id] +
				       MT7925_MLO_RSSI_STALE_JIFFIES))
			goto no_data;

		score += mvif->mlo_link_rssi[link_id];
	}

	*rssi_score = score;
	*bonus = mt7925_mlo_pair_bonus(vif, pair);
	return score + *bonus;

no_data:
	*rssi_score = S32_MIN;
	*bonus = S32_MIN;
	return S32_MIN;
}

static void mt7925_mlo_rotate_rssi_monitor(struct mt792x_dev *dev,
					   struct ieee80211_vif *vif);

void mt7925_mlo_steer_work(struct work_struct *work)
{
	struct mt792x_vif *mvif = container_of(to_delayed_work(work),
					       struct mt792x_vif,
					       mlo_steer_work);
	struct ieee80211_vif *vif =
		container_of((void *)mvif, struct ieee80211_vif, drv_priv);
	struct mt792x_dev *dev = mvif->phy->dev;
	u16 current_links = vif->active_links;
	u16 best_pair = current_links;
	s32 best_score = S32_MIN;
	s32 cur_score;
	int i;

	if (!mvif->mlo_auto_steering || !vif->cfg.assoc ||
	    !ieee80211_vif_is_mld(vif))
		goto requeue;

	mt7925_mlo_rotate_rssi_monitor(dev, vif);

	{
		s32 rssi_ignored, bonus_ignored;

		cur_score = mt7925_mlo_pair_score_parts(vif, current_links,
							&rssi_ignored, &bonus_ignored);
	}

	for (i = 0; i < ARRAY_SIZE(mt7925_mlo_steer_pairs); i++) {
		u16 pair = mt7925_mlo_steer_pairs[i];
		s32 rssi_score, bonus, score;
		unsigned long link_bits = pair;
		int link_id;

		score = mt7925_mlo_pair_score_parts(vif, pair, &rssi_score, &bonus);

		dev_info(dev->mt76.dev, "MLO_STEER: candidate=0x%x\n", pair);
		for_each_set_bit(link_id, &link_bits, IEEE80211_MLD_MAX_NUM_LINKS) {
			enum nl80211_band band = mt7925_mlo_link_band(vif, link_id);
			const char *band_str = band == NL80211_BAND_2GHZ ? "2.4GHz" :
						band == NL80211_BAND_5GHZ ? "5GHz" :
						band == NL80211_BAND_6GHZ ? "6GHz" : "unknown";

			dev_info(dev->mt76.dev, "  link%d band=%s\n", link_id, band_str);
		}
		if (score == S32_MIN)
			dev_info(dev->mt76.dev,
				"  rssi_score=no-data pair_bonus=no-data final_score=no-data\n");
		else
			dev_info(dev->mt76.dev,
				"  rssi_score=%d pair_bonus=%d final_score=%d\n",
				rssi_score, bonus, score);

		if (score > best_score) {
			best_score = score;
			best_pair = pair;
		}
	}

	dev_info(dev->mt76.dev, "MLO_STEER: selected=0x%x\n", best_pair);

	dev_info(dev->mt76.dev, "MLO_STEER:\n");
	dev_info(dev->mt76.dev, "current=0x%x\n", current_links);
	if (best_pair != current_links)
		dev_info(dev->mt76.dev, "candidate=0x%x\n", best_pair);
	if (cur_score == S32_MIN)
		dev_info(dev->mt76.dev, "old_score=no-data\n");
	else
		dev_info(dev->mt76.dev, "old_score=%d\n", cur_score);
	if (best_score == S32_MIN)
		dev_info(dev->mt76.dev, "new_score=no-data\n");
	else
		dev_info(dev->mt76.dev, "new_score=%d\n", best_score);

	if (best_pair != current_links &&
	    best_score != S32_MIN && cur_score != S32_MIN &&
	    best_score >= cur_score + MT7925_MLO_STEER_HYSTERESIS_DB &&
	    time_after(jiffies, mvif->mlo_last_switch_jiffies +
			       MT7925_MLO_STEER_COOLDOWN)) {
		dev_info(dev->mt76.dev,
			"MLO_STEER: switching=0x%x reason=better_score\n",
			best_pair);
		mvif->mlo_last_switch_jiffies = jiffies;
		ieee80211_set_active_links_async(vif, best_pair);
	}

requeue:
	ieee80211_queue_delayed_work(mt76_hw(dev), &mvif->mlo_steer_work,
				     MT7925_MLO_STEER_INTERVAL);
}

/* Disarms whichever link the MLO-steering RSSI monitor currently targets.
 * Bypasses cfg80211 CQM entirely -- this is a driver-internal arm, not a
 * userspace one, and works whether or not userspace ever configures CQM.
 */
static void mt7925_mlo_disarm_rssi_monitor(struct mt792x_dev *dev,
					   struct ieee80211_vif *vif)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct ieee80211_bss_conf *link_conf;

	if (!mvif->mlo_rssi_monitor_enabled)
		return;

	link_conf = mt792x_vif_to_bss_conf(vif, mvif->mlo_rssi_monitor_link_id);
	if (link_conf)
		mt7925_mcu_set_mlo_rssi_monitor(dev, link_conf, false);
	mvif->mlo_rssi_monitor_enabled = false;
}

/* Advances the MLO-steering RSSI monitor to the next valid link (round
 * robin), so every link's RSSI gets refreshed in turn -- only one link's
 * monitor can be armed at a time (see mt7925_mcu_set_mlo_rssi_monitor()).
 * Called once per mt7925_mlo_steer_work() tick while steering is enabled.
 */
static void mt7925_mlo_rotate_rssi_monitor(struct mt792x_dev *dev,
					   struct ieee80211_vif *vif)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct ieee80211_bss_conf *link_conf;
	int link_id = mvif->mlo_rssi_monitor_link_id;
	int i;

	for (i = 0; i < IEEE80211_MLD_MAX_NUM_LINKS; i++) {
		link_id = (link_id + 1) % IEEE80211_MLD_MAX_NUM_LINKS;
		if (mvif->valid_links & BIT(link_id))
			break;
	}
	if (!(mvif->valid_links & BIT(link_id)))
		return;

	mt7925_mlo_disarm_rssi_monitor(dev, vif);

	link_conf = mt792x_vif_to_bss_conf(vif, link_id);
	if (!link_conf)
		return;

	if (!mt7925_mcu_set_mlo_rssi_monitor(dev, link_conf, true)) {
		mvif->mlo_rssi_monitor_link_id = link_id;
		mvif->mlo_rssi_monitor_enabled = true;
	}
}

/* Re-evaluate whether the MLO-steering RSSI monitor should be running at
 * all: only while associated, MLD, and mlo_auto_steering is on. Call after
 * any of those three conditions change (association, interface removal,
 * or the mlo_auto_steering debugfs toggle) to disarm promptly -- rotation
 * itself is driven by mt7925_mlo_steer_work()'s own tick.
 */
void mt7925_mlo_update_rssi_monitor(struct mt792x_dev *dev,
				    struct ieee80211_vif *vif)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	bool want = mvif->mlo_auto_steering && vif->cfg.assoc &&
		    ieee80211_vif_is_mld(vif);

	if (!want)
		mt7925_mlo_disarm_rssi_monitor(dev, vif);
}

static void mt7925_roc_iter(void *priv, u8 *mac,
			    struct ieee80211_vif *vif)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct mt792x_phy *phy = priv;
	u16 mlo_links = phy->roc_mlo_links;
	unsigned int link_id;

	if (!mlo_links || !(mt7925_mlo_roc_release & 1)) {
		if (READ_ONCE(mt76_mlo_diag_trace))
			printk(KERN_INFO "ROC_ABORT: site=roc_iter_early token=%u mlo_links=0x%x\n",
			       phy->roc_token_id, mlo_links);
		mt7925_mcu_abort_roc(phy, &mvif->bss_conf, phy->roc_token_id);
		return;
	}

	/* A multi-link ROC reserves one slot per affiliated link. Releasing
	 * only the link that carried the UNI_ROC_ACQUIRE leaves the
	 * UNI_ROC_SUB_LINK reservation held in firmware, which then drops
	 * every subsequent multi-link ROC request without answering it.
	 */
	for_each_set_bit(link_id, (unsigned long *)&mlo_links,
			 IEEE80211_MLD_MAX_NUM_LINKS) {
		struct mt792x_bss_conf *mconf;

		mconf = mt792x_vif_to_link(mvif, link_id);
		if (!mconf)
			continue;

		if (READ_ONCE(mt76_mlo_diag_trace))
			printk(KERN_INFO "ROC_ABORT: site=roc_iter_mlo_link link_id=%u token=%u dbdcband=0xfe\n",
			       link_id, phy->roc_token_id);

		/* Must match the dbdcband the JOIN was granted under
		 * (0xfe) -- the generic mt7925_mcu_abort_roc() sends 0xff
		 * ("auto"), which does not release a JOIN reservation.
		 */
		mt7925_mcu_abort_mlo_roc(phy, mconf, phy->roc_token_id);
	}

	phy->roc_mlo_links = 0;
}

void mt7925_roc_abort_sync(struct mt792x_dev *dev)
{
	struct mt792x_phy *phy = &dev->phy;

	if (!test_and_clear_bit(MT76_STATE_ROC, &phy->mt76->state))
		return;

	timer_delete_sync(&phy->roc_timer);

	cancel_work(&phy->roc_work);

	ieee80211_iterate_interfaces(mt76_hw(dev),
				     IEEE80211_IFACE_ITER_RESUME_ALL,
				     mt7925_roc_iter, (void *)phy);
}
EXPORT_SYMBOL_GPL(mt7925_roc_abort_sync);

void mt7925_roc_work(struct work_struct *work)
{
	struct mt792x_phy *phy;

	phy = (struct mt792x_phy *)container_of(work, struct mt792x_phy,
						roc_work);

	if (!test_and_clear_bit(MT76_STATE_ROC, &phy->mt76->state))
		return;

	if (READ_ONCE(mt76_mlo_diag_trace))
		printk(KERN_INFO "ROC_WORK_RUN: site=roc_work token=%u mlo_links=0x%x\n",
		       phy->roc_token_id, phy->roc_mlo_links);

	mt792x_mutex_acquire(phy->dev);
	ieee80211_iterate_active_interfaces(phy->mt76->hw,
					    IEEE80211_IFACE_ITER_RESUME_ALL,
					    mt7925_roc_iter, phy);
	mt792x_mutex_release(phy->dev);
	ieee80211_remain_on_channel_expired(phy->mt76->hw);
}

static int mt7925_abort_roc(struct mt792x_phy *phy,
			    struct mt792x_bss_conf *mconf)
{
	int err = 0;

	timer_delete_sync(&phy->roc_timer);
	cancel_work_sync(&phy->roc_work);

	mt792x_mutex_acquire(phy->dev);
	if (test_and_clear_bit(MT76_STATE_ROC, &phy->mt76->state))
		err = mt7925_mcu_abort_roc(phy, mconf, phy->roc_token_id);
	mt792x_mutex_release(phy->dev);

	return err;
}

static int mt7925_set_roc(struct mt792x_phy *phy,
			  struct mt792x_bss_conf *mconf,
			  struct ieee80211_channel *chan,
			  int duration,
			  enum mt7925_roc_req type)
{
	int err;

	if (test_and_set_bit(MT76_STATE_ROC, &phy->mt76->state))
		return -EBUSY;

	phy->roc_grant = false;
	phy->roc_mlo_links = 0;

	if (READ_ONCE(mt76_mlo_diag_trace))
		printk(KERN_INFO "ROC_REQ_START: site=set_roc token=%u duration=%d\n",
		       phy->roc_token_id + 1, duration);

	err = mt7925_mcu_set_roc(phy, mconf, chan, duration, type,
				 ++phy->roc_token_id);
	if (err < 0) {
		clear_bit(MT76_STATE_ROC, &phy->mt76->state);
		goto out;
	}

	if (!wait_event_timeout(phy->roc_wait, phy->roc_grant, 4 * HZ)) {
		if (READ_ONCE(mt76_mlo_diag_trace))
			printk(KERN_INFO "ROC_TIMEOUT: site=set_roc token=%u\n",
			       phy->roc_token_id);
		mt7925_mcu_abort_roc(phy, mconf, phy->roc_token_id);
		clear_bit(MT76_STATE_ROC, &phy->mt76->state);
		err = -ETIMEDOUT;
	}

out:
	return err;
}

static int mt7925_set_mlo_roc(struct mt792x_phy *phy,
			      struct mt792x_bss_conf *mconf,
			      u16 sel_links)
{
	int err;

	if (test_and_set_bit(MT76_STATE_ROC, &phy->mt76->state))
		return -EBUSY;

	/* The firmware keeps the MLO JOIN reservation alive until it is
	 * explicitly aborted with the token it was granted under. Drop a
	 * stale one from a previous association, otherwise the new JOIN
	 * request is silently never granted.
	 */
	if (phy->mlo_roc_token_id && (mt7925_mlo_roc_release & 2)) {
		mt7925_mcu_abort_mlo_roc(phy, mconf, phy->mlo_roc_token_id);
		phy->mlo_roc_token_id = 0;
	}

	phy->roc_grant = false;
	phy->roc_mlo_links = sel_links;

	if (READ_ONCE(mt76_mlo_diag_trace))
		printk(KERN_INFO "ROC_REQ_START: site=set_mlo_roc token=%u sel_links=0x%x duration=5\n",
		       phy->roc_token_id + 1, sel_links);

	err = mt7925_mcu_set_mlo_roc(phy, mconf, sel_links, 5, ++phy->roc_token_id);
	if (err < 0) {
		phy->roc_mlo_links = 0;
		clear_bit(MT76_STATE_ROC, &phy->mt76->state);
		goto out;
	}

	if (!wait_event_timeout(phy->roc_wait, phy->roc_grant, 4 * HZ)) {
		dev_warn(phy->dev->mt76.dev,
			 "multi-link ROC not granted (links 0x%x, token %d)\n",
			 sel_links, phy->roc_token_id);
		if (READ_ONCE(mt76_mlo_diag_trace))
			printk(KERN_INFO "ROC_TIMEOUT: site=set_mlo_roc token=%u sel_links=0x%x\n",
			       phy->roc_token_id, sel_links);
		mt7925_mcu_abort_roc(phy, mconf, phy->roc_token_id);
		phy->roc_mlo_links = 0;
		clear_bit(MT76_STATE_ROC, &phy->mt76->state);
		err = -ETIMEDOUT;
	} else {
		phy->mlo_roc_token_id = phy->roc_token_id;
	}

out:
	return err;
}

static int mt7925_remain_on_channel(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif,
				    struct ieee80211_channel *chan,
				    int duration,
				    enum ieee80211_roc_type type)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct mt792x_phy *phy = mt792x_hw_phy(hw);
	int err;

	mt792x_mutex_acquire(phy->dev);
	err = mt7925_set_roc(phy, &mvif->bss_conf,
			     chan, duration, MT7925_ROC_REQ_ROC);
	mt792x_mutex_release(phy->dev);

	return err;
}

static int mt7925_cancel_remain_on_channel(struct ieee80211_hw *hw,
					   struct ieee80211_vif *vif)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct mt792x_phy *phy = mt792x_hw_phy(hw);

	return mt7925_abort_roc(phy, &mvif->bss_conf);
}

static int mt7925_set_link_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
			       struct ieee80211_vif *vif, struct ieee80211_sta *sta,
			       struct ieee80211_key_conf *key, int link_id,
			       struct mt792x_link_sta *mlink)
{
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct mt792x_sta *msta = sta ? (struct mt792x_sta *)sta->drv_priv :
				  &mvif->sta;
	struct ieee80211_bss_conf *link_conf;
	struct ieee80211_link_sta *link_sta;
	int idx = key->keyidx, err = 0;
	struct mt792x_bss_conf *mconf;
	struct mt76_wcid *wcid;
	u8 *wcid_keyidx;

	link_conf = mt792x_vif_to_bss_conf(vif, link_id);
	link_sta = sta ? mt792x_sta_to_link_sta(vif, sta, link_id) : NULL;
	mconf = mt792x_vif_to_link(mvif, link_id);
	wcid = &mlink->wcid;
	wcid_keyidx = &wcid->hw_key_idx;

	/* fall back to sw encryption for unsupported ciphers */
	switch (key->cipher) {
	case WLAN_CIPHER_SUITE_AES_CMAC:
		key->flags |= IEEE80211_KEY_FLAG_GENERATE_MMIE;
		wcid_keyidx = &wcid->hw_key_idx2;
		break;
	case WLAN_CIPHER_SUITE_WEP40:
	case WLAN_CIPHER_SUITE_WEP104:
		if (!mvif->wep_sta)
			return -EOPNOTSUPP;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
	case WLAN_CIPHER_SUITE_CCMP:
	case WLAN_CIPHER_SUITE_CCMP_256:
	case WLAN_CIPHER_SUITE_GCMP:
	case WLAN_CIPHER_SUITE_GCMP_256:
	case WLAN_CIPHER_SUITE_SMS4:
		break;
	default:
		return -EOPNOTSUPP;
	}

	if (cmd == SET_KEY && !mconf->mt76.cipher) {
		struct mt792x_phy *phy = mt792x_hw_phy(hw);

		mconf->mt76.cipher = mt7925_mcu_get_cipher(key->cipher);
		mt7925_mcu_add_bss_info(phy, mconf->mt76.ctx, link_conf,
					link_sta, true);
	}

	if (cmd == SET_KEY)
		*wcid_keyidx = idx;
	else if (idx == *wcid_keyidx)
		*wcid_keyidx = -1;
	else
		goto out;

	mt76_wcid_key_setup(&dev->mt76, wcid,
			    cmd == SET_KEY ? key : NULL);

	err = mt7925_mcu_add_key(&dev->mt76, vif, &mlink->bip,
				 key, MCU_UNI_CMD(STA_REC_UPDATE),
				 &mlink->wcid, cmd, msta);

	if (err)
		goto out;

	if (key->cipher == WLAN_CIPHER_SUITE_WEP104 ||
	    key->cipher == WLAN_CIPHER_SUITE_WEP40)
		err = mt7925_mcu_add_key(&dev->mt76, vif, &mvif->wep_sta->deflink.bip,
					 key, MCU_WMWA_UNI_CMD(STA_REC_UPDATE),
					 &mvif->wep_sta->deflink.wcid, cmd, msta);
out:
	return err;
}

static int mt7925_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
			  struct ieee80211_vif *vif, struct ieee80211_sta *sta,
			  struct ieee80211_key_conf *key)
{
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct mt792x_sta *msta = sta ? (struct mt792x_sta *)sta->drv_priv :
				  &mvif->sta;
	struct mt792x_link_sta *mlink;
	int err;

	/* The hardware does not support per-STA RX GTK, fallback
	 * to software mode for these.
	 */
	if ((vif->type == NL80211_IFTYPE_ADHOC ||
	     vif->type == NL80211_IFTYPE_MESH_POINT) &&
	    (key->cipher == WLAN_CIPHER_SUITE_TKIP ||
	     key->cipher == WLAN_CIPHER_SUITE_CCMP) &&
	    !(key->flags & IEEE80211_KEY_FLAG_PAIRWISE))
		return -EOPNOTSUPP;

	mt792x_mutex_acquire(dev);

	if (READ_ONCE(mt76_mlo_diag_trace))
		printk(KERN_INFO
		      "MLO_KEY_INSTALL: cmd=%d cipher=0x%x key_link_id=%d\n",
		      cmd, key->cipher, key->link_id);

	if (cmd == SET_KEY)
		mt76_mlo_bssinfo_deferred_send();

	if (ieee80211_vif_is_mld(vif)) {
		unsigned int link_id;
		unsigned long add;

		add = key->link_id != -1 ? BIT(key->link_id) : msta->valid_links;

		for_each_set_bit(link_id, &add, IEEE80211_MLD_MAX_NUM_LINKS) {
			mlink = mt792x_sta_to_link(msta, link_id);
			err = mt7925_set_link_key(hw, cmd, vif, sta, key, link_id,
						  mlink);
			if (err < 0)
				break;
		}
	} else {
		mlink = mt792x_sta_to_link(msta, vif->bss_conf.link_id);
		err = mt7925_set_link_key(hw, cmd, vif, sta, key,
					  vif->bss_conf.link_id, mlink);
	}

	mt792x_mutex_release(dev);

	return err;
}

static void
mt7925_pm_interface_iter(void *priv, u8 *mac, struct ieee80211_vif *vif)
{
	struct mt792x_dev *dev = priv;
	struct ieee80211_hw *hw = mt76_hw(dev);
	/* Monitor vifs must never have beacon filtering / PM applied. */
	bool pm_enable = dev->pm.enable && vif->type != NL80211_IFTYPE_MONITOR;
	int err;

	err = mt7925_mcu_set_beacon_filter(dev, vif, pm_enable);
	if (err < 0)
		return;

	if (pm_enable) {
		vif->driver_flags |= IEEE80211_VIF_BEACON_FILTER;
		ieee80211_hw_set(hw, CONNECTION_MONITOR);
	} else {
		vif->driver_flags &= ~IEEE80211_VIF_BEACON_FILTER;
		__clear_bit(IEEE80211_HW_CONNECTION_MONITOR, hw->flags);
	}
}

static void
mt7925_monitor_update_chan(struct mt792x_vif *mvif,
			   struct ieee80211_chanctx_conf *ctx)
{
	struct mt792x_bss_conf *mconf = &mvif->bss_conf;
	struct ieee80211_channel *chan;

	if (!is_mt7927(&mvif->phy->dev->mt76))
		return;

	chan = ctx ? ctx->def.chan : mvif->phy->mt76->chandef.chan;
	if (!chan)
		return;

	if (ctx)
		mvif->phy->mt76->chandef = ctx->def;

	mconf->mt76.band_idx = mt7927_band_idx(chan->band);
	mconf->mt76.basic_rates_idx = MT792x_BASIC_RATES_TBL;
	if (chan->band != NL80211_BAND_2GHZ)
		mconf->mt76.basic_rates_idx += 4;
}

/* Re-arm the firmware sniffer for a monitor vif on the channel context @ctx.
 * Shared by the assign-chanctx and change-chanctx paths so monitor mode
 * follows channel/band changes across 2.4/5/6 GHz, not just the band it was
 * first enabled on. Caller must hold dev->mt76.mutex.
 */
static void
mt7925_monitor_arm_sniffer(struct mt792x_phy *phy, struct ieee80211_vif *vif,
			   struct ieee80211_chanctx_conf *ctx)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct mt792x_bss_conf *mconf = &mvif->bss_conf;
	struct mt792x_dev *dev = phy->dev;

	/* On a cross-band move, disable the sniffer on the old physical band
	 * first so the firmware re-arms cleanly on the new one.
	 *
	 * mt7925_mcu_set_sniffer() resolves its band_idx from
	 * mvif->bss_conf.mt76.ctx (falling back to the PHY chandef only when
	 * that pointer is NULL), not from an argument we control. On the
	 * assign_vif_chanctx path the caller may have already pointed
	 * mconf->mt76.ctx at the new ctx before calling in here, and on
	 * change_chanctx @ctx is the same object mac80211 already mutated to
	 * the new channel. Either way, calling mt7925_mcu_set_sniffer()
	 * as-is would report the *new* band on what is supposed to be the
	 * old-band teardown. Temporarily clear the ctx pointer so the lookup
	 * falls back to phy->mt76->chandef.chan, which mt7925_monitor_update_chan()
	 * below hasn't overwritten yet and is therefore still the old band.
	 */
	if (is_mt7927(&dev->mt76) && phy->mt76->chandef.chan && ctx->def.chan &&
	    mt7927_band_idx(phy->mt76->chandef.chan->band) !=
	    mt7927_band_idx(ctx->def.chan->band)) {
		struct ieee80211_chanctx_conf *old_ctx = mconf->mt76.ctx;

		mconf->mt76.ctx = NULL;
		mt7925_mcu_set_sniffer(dev, vif, false);
		mconf->mt76.ctx = old_ctx;
	}

	mt7925_monitor_update_chan(mvif, ctx);
	mconf->mt76.ctx = ctx;
	mt7925_mcu_set_sniffer(dev, vif, true);
	mt7925_mcu_config_sniffer(mvif, ctx);
	/* A sniffer must see other-BSS beacons; never beacon-filter it. */
	mt7925_mcu_set_beacon_filter(dev, vif, false);
}

static void
mt7925_sniffer_interface_iter(void *priv, u8 *mac, struct ieee80211_vif *vif)
{
	struct mt792x_dev *dev = priv;
	struct ieee80211_hw *hw = mt76_hw(dev);
	struct mt76_connac_pm *pm = &dev->pm;
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct ieee80211_chanctx_conf *ctx = mvif->bss_conf.mt76.ctx;
	bool monitor = !!(hw->conf.flags & IEEE80211_CONF_MONITOR);

	if (monitor && !ctx)
		return;

	if (monitor)
		mt7925_monitor_update_chan(mvif, ctx);

	mt7925_mcu_set_sniffer(dev, vif, monitor);
	if (monitor && is_mt7927(&dev->mt76))
		mt7925_mcu_config_sniffer(mvif, ctx);
	pm->enable = pm->enable_user && !monitor;
	pm->ds_enable = pm->ds_enable_user && !monitor;

	mt7925_mcu_set_deep_sleep(dev, pm->ds_enable);

	if (monitor)
		mt7925_mcu_set_beacon_filter(dev, vif, false);
}

void mt7925_set_runtime_pm(struct mt792x_dev *dev)
{
	struct ieee80211_hw *hw = mt76_hw(dev);
	struct mt76_connac_pm *pm = &dev->pm;
	bool monitor = !!(hw->conf.flags & IEEE80211_CONF_MONITOR);

	pm->enable = pm->enable_user && !monitor;
	ieee80211_iterate_active_interfaces(hw,
					    IEEE80211_IFACE_ITER_RESUME_ALL,
					    mt7925_pm_interface_iter, dev);
	pm->ds_enable = pm->ds_enable_user && !monitor;
	mt7925_mcu_set_deep_sleep(dev, pm->ds_enable);
}

/* compat: radio_idx added to ieee80211_ops in kernel 6.17 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 17, 0)
static int mt7925_config(struct ieee80211_hw *hw, int radio_idx, u32 changed)
#else
static int mt7925_config(struct ieee80211_hw *hw, u32 changed)
#endif
{
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	int ret = 0;

	mt792x_mutex_acquire(dev);

	if (changed & IEEE80211_CONF_CHANGE_POWER) {
		ret = mt7925_set_tx_sar_pwr(hw, NULL);
		if (ret)
			goto out;
	}

	if (changed & IEEE80211_CONF_CHANGE_MONITOR) {
		ieee80211_iterate_active_interfaces(hw,
						    IEEE80211_IFACE_ITER_RESUME_ALL,
						    mt7925_sniffer_interface_iter, dev);
	}

out:
	mt792x_mutex_release(dev);

	return ret;
}

static void mt7925_configure_filter(struct ieee80211_hw *hw,
				    unsigned int changed_flags,
				    unsigned int *total_flags,
				    u64 multicast)
{
#define MT7925_FILTER_FCSFAIL    BIT(2)
#define MT7925_FILTER_CONTROL    BIT(5)
#define MT7925_FILTER_OTHER_BSS  BIT(6)
#define MT7925_FILTER_ENABLE     BIT(31)
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	u32 flags = MT7925_FILTER_ENABLE;

#define MT7925_FILTER(_fif, _type) do {			\
		if (*total_flags & (_fif))		\
			flags |= MT7925_FILTER_##_type;	\
	} while (0)

	MT7925_FILTER(FIF_FCSFAIL, FCSFAIL);
	MT7925_FILTER(FIF_CONTROL, CONTROL);
	MT7925_FILTER(FIF_OTHER_BSS, OTHER_BSS);

	mt792x_mutex_acquire(dev);
	mt7925_mcu_set_rxfilter(dev, flags, 0, 0);
	if (is_mt7927(&dev->mt76) &&
	    (hw->conf.flags & IEEE80211_CONF_MONITOR))
		ieee80211_iterate_active_interfaces(hw,
					    IEEE80211_IFACE_ITER_RESUME_ALL,
					    mt7925_sniffer_interface_iter, dev);
	mt792x_mutex_release(dev);

	*total_flags &= (FIF_OTHER_BSS | FIF_FCSFAIL | FIF_CONTROL);
}

static u8
mt7925_get_rates_table(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		       bool beacon, bool mcast)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	struct mt76_phy *mphy = hw->priv;
	u16 rate;
	u8 i, idx, ht;

	rate = mt76_connac2_mac_tx_rate_val(mphy, &vif->bss_conf, beacon, mcast);
	ht = FIELD_GET(MT_TX_RATE_MODE, rate) > MT_PHY_TYPE_OFDM;

	if (beacon && ht) {
		struct mt792x_dev *dev = mt792x_hw_dev(hw);

		/* must odd index */
		idx = MT7925_BEACON_RATES_TBL + 2 * (mvif->idx % 20);
		mt7925_mac_set_fixed_rate_table(dev, idx, rate);
		return idx;
	}

	idx = FIELD_GET(MT_TX_RATE_IDX, rate);
	for (i = 0; i < ARRAY_SIZE(mt76_rates); i++)
		if ((mt76_rates[i].hw_value & GENMASK(7, 0)) == idx)
			return MT792x_BASIC_RATES_TBL + i;

	return mvif->basic_rates_idx;
}

static int mt7925_mac_link_sta_add(struct mt76_dev *mdev,
				   struct ieee80211_vif *vif,
				   struct ieee80211_link_sta *link_sta,
				   struct mt792x_link_sta *mlink)
{
	struct mt792x_dev *dev = container_of(mdev, struct mt792x_dev, mt76);
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct ieee80211_bss_conf *link_conf;
	struct mt792x_bss_conf *mconf;
	u8 link_id = link_sta->link_id;
	bool wcid_published = false;
	struct mt792x_sta *msta;
	struct mt76_wcid *wcid;
	bool pm_woken = false;
	int ret, idx;

	msta = (struct mt792x_sta *)link_sta->sta->drv_priv;

	if (WARN_ON_ONCE(!mlink))
		return -EINVAL;

	dev_info(dev->mt76.dev,
		 "EXPERIMENTAL MLO TEST: link_sta_add link_id=%u is_pri=%d valid_links=0x%x\n",
		 link_id, link_sta == mlink->pri_link, link_sta->sta->valid_links);

	idx = mt76_wcid_alloc(dev->mt76.wcid_mask, MT792x_WTBL_STA - 1);
	if (idx < 0) {
		dev_info(dev->mt76.dev,
			 "EXPERIMENTAL MLO TEST: link_id=%u wcid_alloc FAILED (idx=%d)\n",
			 link_id, idx);
		return -ENOSPC;
	}

	mconf = mt792x_vif_to_link(mvif, link_id);
	mt76_wcid_init(&mlink->wcid, 0);
	mlink->wcid.sta = 1;
	mlink->wcid.idx = idx;
	mlink->wcid.tx_info |= MT_WCID_TX_INFO_SET;
	mlink->last_txs = jiffies;
	mlink->wcid.link_id = link_sta->link_id;
	mlink->wcid.link_valid = !!link_sta->sta->valid_links;
	mlink->sta = msta;

	if (link_sta->sta->tdls)
		set_bit(MT_WCID_FLAG_TDLS_PEER, &mlink->wcid.flags);

	wcid = &mlink->wcid;
	ewma_signal_init(&wcid->rssi);
	rcu_assign_pointer(dev->mt76.wcid[wcid->idx], wcid);
	wcid_published = true;
	ewma_avg_signal_init(&mlink->avg_ack_signal);
	memset(mlink->airtime_ac, 0,
	       sizeof(msta->deflink.airtime_ac));

	ret = mt76_connac_pm_wake(&dev->mphy, &dev->pm);
	if (ret)
		goto out_wcid;
	pm_woken = true;

	mt7925_mac_wtbl_update(dev, idx,
			       MT_WTBL_UPDATE_ADM_COUNT_CLEAR);

	link_conf = mt792x_vif_to_bss_conf(vif, link_id);

	/* should update bss info before STA add */
	if (vif->type == NL80211_IFTYPE_STATION && !link_sta->sta->tdls) {
		struct mt792x_link_sta *mlink_bc;

		mlink_bc = mt792x_sta_to_link(&mvif->sta, mconf->link_id);

		if (ieee80211_vif_is_mld(vif)) {
			ret = mt7925_mcu_add_bss_info_sta(&dev->phy, mconf->mt76.ctx,
							  link_conf, link_sta,
							  mlink_bc->wcid.idx, mlink->wcid.idx,
							  link_sta != mlink->pri_link);
			if (ret)
				goto out_pm;
		} else {
			ret = mt7925_mcu_add_bss_info_sta(&dev->phy, mconf->mt76.ctx,
							  link_conf, link_sta,
							  mlink_bc->wcid.idx, mlink->wcid.idx,
							  false);
			if (ret)
				goto out_pm;
		}
	}

	if (ieee80211_vif_is_mld(vif) &&
	    link_sta == mlink->pri_link) {
		ret = mt7925_mcu_sta_update(dev, link_sta, vif,
					    mlink, true,
					    MT76_STA_INFO_STATE_NONE);
		if (ret)
			goto out_pm;
	} else if (ieee80211_vif_is_mld(vif) &&
		   link_sta != mlink->pri_link) {
		struct mt792x_link_sta *pri_mlink;

		/* deflink_id is the authoritative primary-link source of
		 * truth (can be promoted away from the original def_wcid
		 * embedded-storage link by failover -- see
		 * mt7925_mac_sta_remove_links()). def_wcid itself is a fixed
		 * address set once at link-add time and does not follow
		 * promotion, so it must not be used to find "the primary."
		 */
		pri_mlink = mt792x_sta_to_link(msta, msta->deflink_id);

		if (WARN_ON_ONCE(!pri_mlink)) {
			ret = -EINVAL;
			goto out_pm;
		}

		if (READ_ONCE(mt76_mlo_diag_trace))
			printk(KERN_INFO
			      "MLO_STA_REC_PRIMARY_RETOUCH: adding_link=%u pri_wcid=%u pri_link_id=%u state=ASSOC\n",
			      link_id, pri_mlink->wcid.idx, pri_mlink->wcid.link_id);

		ret = mt7925_mcu_sta_update(dev, mlink->pri_link, vif,
					    pri_mlink, true,
					    MT76_STA_INFO_STATE_ASSOC);
		if (ret)
			goto out_pm;

		{
			enum mt76_sta_info_state secondary_state = MT76_STA_INFO_STATE_ASSOC;

			if (mt76_mlo_bssadd_skip == 13) {
				if (READ_ONCE(mt76_mlo_diag_trace))
					printk(KERN_INFO
					      "MLO_STAREC_STATE_OVERRIDE: link_id=%u old_state=%u new_state=%u\n",
					      link_id, MT76_STA_INFO_STATE_ASSOC,
					      MT76_STA_INFO_STATE_NONE);
				secondary_state = MT76_STA_INFO_STATE_NONE;
			}

			if (READ_ONCE(mt76_mlo_diag_trace))
				printk(KERN_INFO
				      "MLO_STA_REC_SECONDARY: link_id=%u wcid=%u state=%u\n",
				      link_id, mlink->wcid.idx, secondary_state);

			ret = mt7925_mcu_sta_update(dev, link_sta, vif,
						    mlink, true, secondary_state);
			if (ret)
				goto out_pm;
		}
	} else {
		ret = mt7925_mcu_sta_update(dev, link_sta, vif,
					    mlink, true,
					    MT76_STA_INFO_STATE_NONE);
		if (ret)
			goto out_pm;
	}

	mt76_connac_power_save_sched(&dev->mphy, &dev->pm);

	dev_info(dev->mt76.dev,
		 "EXPERIMENTAL MLO TEST: link_id=%u wcid_idx=%u published OK\n",
		 link_id, idx);

	/* Restored (2026-08-23): stamp this link's readiness time so
	 * mt7925_mlo_link_tx_stable() (mt792x_core.c) can withhold it from
	 * bulk TX for MT7925_MLO_TX_STABILIZE_MS after publish.
	 */
	msta->mlo_link_ready_jiffies[link_id] = jiffies;
	if (READ_ONCE(mt76_mlo_diag_trace))
		printk(KERN_INFO
		      "MLO_LINK_READY: link_id=%u wcid=%u active_links=0x%x\n",
		      link_id, idx, vif->active_links);

	return 0;

out_pm:
	if (pm_woken)
		mt76_connac_power_save_sched(&dev->mphy, &dev->pm);
out_wcid:
	dev_info(dev->mt76.dev,
		 "EXPERIMENTAL MLO TEST: link_id=%u FAILED ret=%d, unwinding wcid_idx=%u\n",
		 link_id, ret, wcid_published ? wcid->idx : 0xffff);
	if (wcid_published) {
		u16 idx = wcid->idx;

		rcu_assign_pointer(dev->mt76.wcid[idx], NULL);
		mt76_wcid_cleanup(mdev, wcid);
		mt76_wcid_mask_clear(mdev->wcid_mask, wcid->idx);
	}
	return ret;
}

/*
 * Host-only unwind for sta_add_links() failures.
 *
 * If add_links fail due to MCU/firmware timeouts; calling the full remove
 * path would send more firmware commands and may hang again. So only rollback
 * host-published state here (msta->link/valid_links, dev->mt76.wcid[idx]) and
 * free mlink objects (RCU-safe). Firmware state is left for reset/recovery.
 */
static void
mt7925_mac_sta_unwind_links_host(struct mt792x_dev *dev,
				 struct ieee80211_sta *sta,
				 unsigned long links)
{
	struct mt792x_sta *msta = (struct mt792x_sta *)sta->drv_priv;
	unsigned int link_id;

	for_each_set_bit(link_id, &links, IEEE80211_MLD_MAX_NUM_LINKS) {
		struct mt792x_link_sta *mlink;
		u16 idx;

		mlink = rcu_replace_pointer(msta->link[link_id], NULL,
					    lockdep_is_held(&dev->mt76.mutex));
		if (!mlink)
			continue;

		msta->valid_links &= ~BIT(link_id);
		if (msta->deflink_id == link_id) {
			if (msta->seclink_id == link_id ||
			    !mt792x_sta_to_link(msta, msta->seclink_id)) {
				msta->deflink_id = IEEE80211_LINK_UNSPECIFIED;
				msta->seclink_id = IEEE80211_LINK_UNSPECIFIED;
			} else {
				msta->deflink_id = msta->seclink_id;
			}
		} else if (msta->seclink_id == link_id) {
			msta->seclink_id = msta->deflink_id;
		}

		idx = mlink->wcid.idx;
		rcu_assign_pointer(dev->mt76.wcid[idx], NULL);
		mt76_wcid_cleanup(&dev->mt76, &mlink->wcid);
		mt76_wcid_mask_clear(dev->mt76.wcid_mask, idx);

		if (mlink != &msta->deflink)
			kfree_rcu(mlink, rcu_head);
	}
}

static int
mt7925_mac_sta_add_links(struct mt792x_dev *dev, struct ieee80211_vif *vif,
			 struct ieee80211_sta *sta, unsigned long new_links)
{
	struct mt792x_sta *msta = (struct mt792x_sta *)sta->drv_priv;
	unsigned long added_links = 0;
	unsigned int link_id;
	int err = 0;

	for_each_set_bit(link_id, &new_links, IEEE80211_MLD_MAX_NUM_LINKS) {
		struct ieee80211_link_sta *link_sta;
		struct mt792x_link_sta *mlink;
		bool is_deflink = false;

		if (msta->deflink_id == IEEE80211_LINK_UNSPECIFIED) {
			mlink = &msta->deflink;
			is_deflink = true;
		} else {
			mlink = kzalloc(sizeof(*mlink), GFP_KERNEL);
			if (!mlink) {
				err = -ENOMEM;
				break;
			}
		}

		mlink->sta = msta;
		mlink->pri_link = &sta->deflink;
		mlink->wcid.def_wcid = &msta->deflink.wcid;

		link_sta = mt792x_sta_to_link_sta(vif, sta, link_id);
		err = mt7925_mac_link_sta_add(&dev->mt76, vif, link_sta, mlink);
		if (err) {
			if (!is_deflink)
				kfree_rcu(mlink, rcu_head);
			break;
		}

		if (is_deflink) {
			msta->deflink_id = link_id;
			msta->seclink_id = link_id;
		} else if (msta->seclink_id == msta->deflink_id) {
			/* no real backup yet -- this newly-added link becomes
			 * the failover candidate (mirrors mt7996/main.c:1174-1177)
			 */
			msta->seclink_id = link_id;
		}

		rcu_assign_pointer(msta->link[link_id], mlink);
		msta->valid_links |= BIT(link_id);

		added_links |= BIT(link_id);
	}

	if (err && added_links)
		mt7925_mac_sta_unwind_links_host(dev, sta, added_links);

	return err;
}

int mt7925_mac_sta_add(struct mt76_dev *mdev, struct ieee80211_vif *vif,
		       struct ieee80211_sta *sta)
{
	struct mt792x_dev *dev = container_of(mdev, struct mt792x_dev, mt76);
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct mt792x_sta *msta = (struct mt792x_sta *)sta->drv_priv;
	int err;

	msta->vif = mvif;

	if (vif->type == NL80211_IFTYPE_STATION)
		mvif->wep_sta = msta;

	if (ieee80211_vif_is_mld(vif)) {
		msta->deflink_id = IEEE80211_LINK_UNSPECIFIED;

		err = mt7925_mac_sta_add_links(dev, vif, sta, sta->valid_links);
	} else {
		err = mt7925_mac_link_sta_add(mdev, vif, &sta->deflink,
					      &msta->deflink);
	}

	return err;
}
EXPORT_SYMBOL_GPL(mt7925_mac_sta_add);

/* TEST-ONLY: deterministically choose an already-usable MT7927 MLO pair
 * before link activation. Zero preserves the automatic selector.
 */
static u8 mt7925_test_mlo_pair_mask;
module_param_named(test_mt7927_mlo_pair_mask, mt7925_test_mlo_pair_mask,
		   byte, 0644);
MODULE_PARM_DESC(test_mt7927_mlo_pair_mask,
		 "TEST-ONLY MT7927 pre-activation MLO pair mask (0, 0x3, 0x5, or 0x6)");

static void
mt7925_mac_set_links(struct mt76_dev *mdev, struct ieee80211_vif *vif)
{
	struct mt792x_dev *dev = container_of(mdev, struct mt792x_dev, mt76);
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct ieee80211_bss_conf *link_conf =
		mt792x_vif_to_bss_conf(vif, mvif->deflink_id);
	struct cfg80211_chan_def *chandef = &link_conf->chanreq.oper;
	enum nl80211_band band = chandef->chan->band, secondary_band;
	u16 sel_links = mt76_select_links(vif, 2);
	u16 test_pair = READ_ONCE(mt7925_test_mlo_pair_mask);
	unsigned long usable_links = ieee80211_vif_usable_links(vif);
	u8 secondary_link_id;

	if (test_pair && is_mt7927(mdev)) {
		if (test_pair != 0x3 && test_pair != 0x5 && test_pair != 0x6) {
			dev_warn(dev->mt76.dev,
				 "MLO_LINK_SELECT: TEST-ONLY MT7927 pair 0x%x invalid; automatic pair remains 0x%x\n",
				 test_pair, sel_links);
		} else if ((test_pair & usable_links) != test_pair) {
			dev_warn(dev->mt76.dev,
				 "MLO_LINK_SELECT: TEST-ONLY MT7927 pair 0x%x not usable (candidates=0x%lx); automatic pair remains 0x%x\n",
				 test_pair, usable_links, sel_links);
		} else if (!(test_pair & BIT(mvif->deflink_id))) {
			dev_warn(dev->mt76.dev,
				 "MLO_LINK_SELECT: TEST-ONLY MT7927 pair 0x%x excludes active anchor link%u; automatic pair remains 0x%x\n",
				 test_pair, mvif->deflink_id, sel_links);
		} else {
			sel_links = test_pair;
			dev_warn(dev->mt76.dev,
				 "MLO_LINK_SELECT: TEST-ONLY MT7927 EHT/MLO pair override selected pair=0x%x anchor=link%u candidates=0x%lx\n",
				 sel_links, mvif->deflink_id, usable_links);
		}
	}

	if (!ieee80211_vif_is_mld(vif) || hweight16(sel_links) < 2)
		return;

	secondary_link_id = __ffs(~BIT(mvif->deflink_id) & sel_links);

	link_conf = mt792x_vif_to_bss_conf(vif, secondary_link_id);
	secondary_band = link_conf->chanreq.oper.chan->band;

	if (band == NL80211_BAND_2GHZ ||
	    (band == NL80211_BAND_5GHZ && secondary_band == NL80211_BAND_6GHZ)) {
		int err;

		mt7925_abort_roc(mvif->phy, &mvif->bss_conf);

		mt792x_mutex_acquire(dev);
		err = mt7925_set_mlo_roc(mvif->phy, &mvif->bss_conf, sel_links);
		mt792x_mutex_release(dev);

		if (err) {
			dev_warn(dev->mt76.dev,
				 "MLO ROC reservation failed (err=%d), not activating link 0x%x\n",
				 err, sel_links);
			return;
		}
	}

	ieee80211_set_active_links_async(vif, sel_links);
}

static void mt7925_mac_link_sta_assoc(struct mt76_dev *mdev,
				      struct ieee80211_vif *vif,
				      struct ieee80211_link_sta *link_sta)
{
	struct mt792x_dev *dev = container_of(mdev, struct mt792x_dev, mt76);
	struct ieee80211_bss_conf *link_conf;
	struct mt792x_link_sta *mlink;
	struct mt792x_sta *msta;

	mt792x_mutex_acquire(dev);

	msta = (struct mt792x_sta *)link_sta->sta->drv_priv;
	mlink = mt792x_sta_to_link(msta, link_sta->link_id);

	if (ieee80211_vif_is_mld(vif)) {
		link_conf = mt792x_vif_to_bss_conf(vif, msta->deflink_id);
	} else {
		link_conf = mt792x_vif_to_bss_conf(vif, vif->bss_conf.link_id);
	}

	if (vif->type == NL80211_IFTYPE_STATION && !link_sta->sta->tdls) {
		struct mt792x_bss_conf *mconf;

		mconf = mt792x_link_conf_to_mconf(link_conf);
		mt7925_mcu_add_bss_info(&dev->phy, mconf->mt76.ctx,
					link_conf, link_sta, true);
	}

	ewma_avg_signal_init(&mlink->avg_ack_signal);

	mt7925_mac_wtbl_update(dev, mlink->wcid.idx,
			       MT_WTBL_UPDATE_ADM_COUNT_CLEAR);
	memset(mlink->airtime_ac, 0, sizeof(mlink->airtime_ac));

	mt7925_mcu_sta_update(dev, link_sta, vif, mlink, true,
			      MT76_STA_INFO_STATE_ASSOC);

	mt792x_mutex_release(dev);
}

int mt7925_mac_sta_event(struct mt76_dev *mdev, struct ieee80211_vif *vif,
			 struct ieee80211_sta *sta, enum mt76_sta_event ev)
{
	struct ieee80211_link_sta *link_sta = &sta->deflink;

	if (ev != MT76_STA_EVENT_ASSOC)
		return 0;

	if (ieee80211_vif_is_mld(vif)) {
		struct mt792x_sta *msta = (struct mt792x_sta *)sta->drv_priv;

		link_sta = mt792x_sta_to_link_sta(vif, sta, msta->deflink_id);
		mt7925_mac_set_links(mdev, vif);
	}

	mt7925_mac_link_sta_assoc(mdev, vif, link_sta);

	return 0;
}
EXPORT_SYMBOL_GPL(mt7925_mac_sta_event);

static void mt7925_mac_link_sta_remove(struct mt76_dev *mdev,
				       struct ieee80211_vif *vif,
				       struct ieee80211_link_sta *link_sta,
				       struct mt792x_link_sta *mlink)
{
	struct mt792x_dev *dev = container_of(mdev, struct mt792x_dev, mt76);
	struct mt76_wcid *wcid = &mlink->wcid;
	struct ieee80211_bss_conf *link_conf;
	u8 link_id = link_sta->link_id;
	u16 idx = wcid->idx;

	mt7925_roc_abort_sync(dev);

	mt76_connac_free_pending_tx_skbs(&dev->pm, wcid);
	mt76_connac_pm_wake(&dev->mphy, &dev->pm);

	mt7925_mcu_sta_update(dev, link_sta, vif, mlink, false,
			      MT76_STA_INFO_STATE_NONE);
	mt7925_mac_wtbl_update(dev, mlink->wcid.idx,
			       MT_WTBL_UPDATE_ADM_COUNT_CLEAR);

	link_conf = mt792x_vif_to_bss_conf(vif, link_id);

	if (vif->type == NL80211_IFTYPE_STATION && !link_sta->sta->tdls) {
		struct mt792x_bss_conf *mconf;

		mconf = mt792x_link_conf_to_mconf(link_conf);

		if (ieee80211_vif_is_mld(vif))
			mt792x_mac_link_bss_remove(dev, mconf, mlink);
		else
			mt7925_mcu_add_bss_info(&dev->phy, mconf->mt76.ctx, link_conf,
						link_sta, false);
	}

	spin_lock_bh(&mdev->sta_poll_lock);
	if (!list_empty(&mlink->wcid.poll_list))
		list_del_init(&mlink->wcid.poll_list);
	spin_unlock_bh(&mdev->sta_poll_lock);

	rcu_assign_pointer(dev->mt76.wcid[idx], NULL);
	mt76_wcid_cleanup(mdev, wcid);
	mt76_wcid_mask_clear(mdev->wcid_mask, idx);

	mt76_connac_power_save_sched(&dev->mphy, &dev->pm);
}

static int
mt7925_mac_sta_remove_links(struct mt792x_dev *dev, struct ieee80211_vif *vif,
			    struct ieee80211_sta *sta, unsigned long old_links)
{
	struct mt792x_sta *msta = (struct mt792x_sta *)sta->drv_priv;
	struct mt76_dev *mdev = &dev->mt76;
	unsigned int link_id;

	/* clean up bss before starec */
	for_each_set_bit(link_id, &old_links, IEEE80211_MLD_MAX_NUM_LINKS) {
		struct ieee80211_link_sta *link_sta;
		struct ieee80211_bss_conf *link_conf;
		struct mt792x_bss_conf *mconf;
		struct mt792x_link_sta *mlink;

		if (vif->type == NL80211_IFTYPE_AP)
			break;

		if (vif->type == NL80211_IFTYPE_STATION && sta->tdls)
			continue;

		link_sta = mt792x_sta_to_link_sta(vif, sta, link_id);
		if (!link_sta)
			continue;

		mlink = mt792x_sta_to_link(msta, link_id);
		if (!mlink)
			continue;

		link_conf = mt792x_vif_to_bss_conf(vif, link_id);
		if (!link_conf)
			continue;

		mconf = mt792x_link_conf_to_mconf(link_conf);

		mt7925_mcu_add_bss_info(&dev->phy, mconf->mt76.ctx, link_conf,
					link_sta, false);
	}

	for_each_set_bit(link_id, &old_links, IEEE80211_MLD_MAX_NUM_LINKS) {
		struct ieee80211_link_sta *link_sta;
		struct mt792x_link_sta *mlink;

		link_sta = mt792x_sta_to_link_sta(vif, sta, link_id);
		if (!link_sta)
			continue;

		mlink = rcu_replace_pointer(msta->link[link_id], NULL,
					    lockdep_is_held(&mdev->mutex));
		if (!mlink)
			continue;

		msta->valid_links &= ~BIT(link_id);
		mlink->sta = NULL;
		mlink->pri_link = NULL;

		/* Promote a backup link BEFORE removing this link's firmware/
		 * mac80211 state below, so mt7925_mac_link_sta_remove() and
		 * any firmware STA_REC update it triggers already see the new
		 * deflink_id, never a stale/removed one. Mirrors mt7996's
		 * promotion pattern (mt7996/main.c:1231-1251), adapted to
		 * mt7925: mt7925 never moves data between the embedded
		 * msta->deflink storage and a separately-allocated mlink --
		 * promoting deflink_id to seclink_id is a plain index update,
		 * since msta->link[] already holds every link's real storage
		 * regardless of which id is "deflink" (see mt792x_core.c and
		 * mt792x.h consumers, which look up via mt792x_sta_to_link(),
		 * not via a fixed embedded pointer).
		 */
		if (msta->deflink_id == link_id) {
			if (msta->seclink_id == link_id) {
				/* no backup was available */
				msta->deflink_id = IEEE80211_LINK_UNSPECIFIED;
				msta->seclink_id = IEEE80211_LINK_UNSPECIFIED;
			} else if (mt792x_sta_to_link(msta, msta->seclink_id)) {
				u8 promoted = msta->seclink_id;

				msta->deflink_id = promoted;
				if (READ_ONCE(mt76_mlo_diag_trace))
					printk(KERN_INFO
					      "MLO_DEFLINK_PROMOTE: sta=%pM old=%u new=%u\n",
					      sta->addr, link_id, promoted);
			} else {
				msta->deflink_id = IEEE80211_LINK_UNSPECIFIED;
			}
		} else if (msta->seclink_id == link_id) {
			/* backup link removed, not the primary -- fall back
			 * to no backup until another link is added
			 */
			msta->seclink_id = msta->deflink_id;
		}

		mt7925_mac_link_sta_remove(&dev->mt76, vif, link_sta, mlink);

		if (mlink != &msta->deflink)
			kfree_rcu(mlink, rcu_head);
	}

	return 0;
}

void mt7925_mac_sta_remove(struct mt76_dev *mdev, struct ieee80211_vif *vif,
			   struct ieee80211_sta *sta)
{
	struct mt792x_dev *dev = container_of(mdev, struct mt792x_dev, mt76);
	struct mt792x_sta *msta = (struct mt792x_sta *)sta->drv_priv;
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;

	if (ieee80211_vif_is_mld(vif)) {
		mt7925_mac_sta_remove_links(dev, vif, sta, msta->valid_links);
		mt7925_mcu_del_dev(mdev, vif);
	} else {
		mt7925_mac_link_sta_remove(mdev, vif, &sta->deflink,
					   &msta->deflink);
	}

	if (vif->type == NL80211_IFTYPE_STATION) {
		mvif->wep_sta = NULL;
		ewma_rssi_init(&mvif->bss_conf.rssi);
	}

	mvif->mlo_pm_state = MT792x_MLO_LINK_DISASSOC;
}
EXPORT_SYMBOL_GPL(mt7925_mac_sta_remove);

/* compat: radio_idx added to ieee80211_ops in kernel 6.17 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 17, 0)
static int mt7925_set_rts_threshold(struct ieee80211_hw *hw, int radio_idx,
				    u32 val)
#else
static int mt7925_set_rts_threshold(struct ieee80211_hw *hw, u32 val)
#endif
{
	struct mt792x_dev *dev = mt792x_hw_dev(hw);

	mt792x_mutex_acquire(dev);
	mt7925_mcu_set_rts_thresh(&dev->phy, val);
	mt792x_mutex_release(dev);

	return 0;
}

static const char *mt7925_ampdu_action_name(enum ieee80211_ampdu_mlme_action action)
{
	switch (action) {
	case IEEE80211_AMPDU_RX_START: return "RX_START";
	case IEEE80211_AMPDU_RX_STOP: return "RX_STOP";
	case IEEE80211_AMPDU_TX_START: return "TX_START";
	case IEEE80211_AMPDU_TX_OPERATIONAL: return "TX_OPERATIONAL";
	case IEEE80211_AMPDU_TX_STOP_FLUSH: return "TX_STOP_FLUSH";
	case IEEE80211_AMPDU_TX_STOP_FLUSH_CONT: return "TX_STOP_FLUSH_CONT";
	case IEEE80211_AMPDU_TX_STOP_CONT: return "TX_STOP_CONT";
	default: return "UNKNOWN";
	}
}

static int
mt7925_ampdu_action(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		    struct ieee80211_ampdu_params *params)
{
	enum ieee80211_ampdu_mlme_action action = params->action;
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	struct ieee80211_sta *sta = params->sta;
	struct ieee80211_txq *txq = sta->txq[params->tid];
	struct mt792x_sta *msta = (struct mt792x_sta *)sta->drv_priv;
	u16 tid = params->tid;
	u16 ssn = params->ssn;
	struct mt76_txq *mtxq;
	int ret = 0;

	if (!txq)
		return -EINVAL;

	mtxq = (struct mt76_txq *)txq->drv_priv;

	mt792x_mutex_acquire(dev);
	switch (action) {
	case IEEE80211_AMPDU_RX_START:
	case IEEE80211_AMPDU_RX_STOP:
	case IEEE80211_AMPDU_TX_START:
	case IEEE80211_AMPDU_TX_OPERATIONAL:
	case IEEE80211_AMPDU_TX_STOP_FLUSH:
	case IEEE80211_AMPDU_TX_STOP_FLUSH_CONT:
	case IEEE80211_AMPDU_TX_STOP_CONT: {
		/* Per-link AMPDU bookkeeping: params carries no link_id
		 * (struct ieee80211_ampdu_params has none -- confirmed,
		 * mac80211.h), so apply the state change to every active
		 * link's own wcid, the same active-link iteration
		 * mt7925_mcu_uni_tx_ba() already uses for the real firmware
		 * BA command. Without this, a TID pinned onto a non-deflink
		 * link (mlo_tid_link) transmits with wcid->ampdu_state still
		 * 0 on that link -- firmware never told the link is
		 * aggregating -- even though mac80211 believes AMPDU is
		 * active for the TID.
		 *
		 * mtxq->aggr/send_bar are deliberately left as single,
		 * non-per-link flags: struct mt76_txq is one object per
		 * (sta, tid), not per link, so there is nothing to iterate
		 * there -- it reflects "is this TID aggregating at all",
		 * which is still correct as a single flag.
		 */
		struct ieee80211_link_sta *link_sta;
		unsigned int link_id;

		for_each_sta_active_link(vif, sta, link_sta, link_id) {
			struct mt792x_link_sta *mlink;

			mlink = mt792x_sta_to_link(msta, link_id);
			if (!mlink)
				continue;

			switch (action) {
			case IEEE80211_AMPDU_RX_START:
				mt76_rx_aggr_start(&dev->mt76, &mlink->wcid, tid,
						   ssn, params->buf_size);
				break;
			case IEEE80211_AMPDU_RX_STOP:
				mt76_rx_aggr_stop(&dev->mt76, &mlink->wcid, tid);
				break;
			case IEEE80211_AMPDU_TX_START:
				set_bit(tid, &mlink->wcid.ampdu_state);
				break;
			case IEEE80211_AMPDU_TX_STOP_FLUSH:
			case IEEE80211_AMPDU_TX_STOP_FLUSH_CONT:
			case IEEE80211_AMPDU_TX_STOP_CONT:
				clear_bit(tid, &mlink->wcid.ampdu_state);
				break;
			default:
				break;
			}

			if (READ_ONCE(mt76_mlo_diag_trace))
				printk(KERN_INFO
				      "MLO_AMPDU_LINK: action=%s tid=%u link=%u wcid=%u state=0x%lx\n",
				      mt7925_ampdu_action_name(action), tid,
				      link_id, mlink->wcid.idx,
				      mlink->wcid.ampdu_state);
		}

		switch (action) {
		case IEEE80211_AMPDU_RX_START:
			mt7925_mcu_uni_rx_ba(dev, params, vif, true);
			break;
		case IEEE80211_AMPDU_RX_STOP:
			mt7925_mcu_uni_rx_ba(dev, params, vif, false);
			break;
		case IEEE80211_AMPDU_TX_OPERATIONAL:
			mtxq->aggr = true;
			mtxq->send_bar = false;
			mt7925_mcu_uni_tx_ba(dev, params, vif, true);
			break;
		case IEEE80211_AMPDU_TX_STOP_FLUSH:
		case IEEE80211_AMPDU_TX_STOP_FLUSH_CONT:
			mtxq->aggr = false;
			mt7925_mcu_uni_tx_ba(dev, params, vif, false);
			mt7925_mlo_clear_tid_link(msta, tid);
			break;
		case IEEE80211_AMPDU_TX_START:
			ret = IEEE80211_AMPDU_TX_START_IMMEDIATE;
			break;
		case IEEE80211_AMPDU_TX_STOP_CONT:
			mtxq->aggr = false;
			mt7925_mcu_uni_tx_ba(dev, params, vif, false);
			mt7925_mlo_clear_tid_link(msta, tid);
			ieee80211_stop_tx_ba_cb_irqsafe(vif, sta->addr, tid);
			break;
		default:
			break;
		}
		break;
	}
	}
	mt792x_mutex_release(dev);

	return ret;
}

static void
mt7925_mlo_pm_iter(void *priv, u8 *mac, struct ieee80211_vif *vif)
{
	struct mt792x_dev *dev = priv;
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	unsigned long valid = ieee80211_vif_is_mld(vif) ?
				    mvif->valid_links : BIT(0);
	struct ieee80211_bss_conf *bss_conf;
	int i;

	if (mvif->mlo_pm_state != MT792x_MLO_CHANGED_PS)
		return;

	mt792x_mutex_acquire(dev);
	for_each_set_bit(i, &valid, IEEE80211_MLD_MAX_NUM_LINKS) {
		bss_conf = mt792x_vif_to_bss_conf(vif, i);
		mt7925_mcu_uni_bss_ps(dev, bss_conf);
	}
	mt792x_mutex_release(dev);
}

void mt7925_mlo_pm_work(struct work_struct *work)
{
	struct mt792x_dev *dev = container_of(work, struct mt792x_dev,
					      mlo_pm_work.work);
	struct ieee80211_hw *hw = mt76_hw(dev);

	ieee80211_iterate_active_interfaces(hw,
					    IEEE80211_IFACE_ITER_RESUME_ALL,
					    mt7925_mlo_pm_iter, dev);
}

void mt7925_scan_work(struct work_struct *work)
{
	struct mt792x_phy *phy;
	struct mt792x_dev *dev;
	struct mt76_connac_pm *pm;

	phy = (struct mt792x_phy *)container_of(work, struct mt792x_phy,
						scan_work.work);

	dev = phy->dev;
	pm = &dev->pm;

	if (pm->suspended)
		return;

	while (true) {
		struct sk_buff *skb;
		struct tlv *tlv;
		int tlv_len;

		spin_lock_bh(&phy->dev->mt76.lock);
		skb = __skb_dequeue(&phy->scan_event_list);
		spin_unlock_bh(&phy->dev->mt76.lock);

		if (!skb)
			break;

		skb_pull(skb, sizeof(struct mt7925_mcu_rxd) + 4);
		tlv = (struct tlv *)skb->data;
		tlv_len = skb->len;

		while (tlv_len > 0 && le16_to_cpu(tlv->len) <= tlv_len) {
			struct mt7925_mcu_scan_chinfo_event *evt;

			switch (le16_to_cpu(tlv->tag)) {
			case UNI_EVENT_SCAN_DONE_BASIC:
				if (test_and_clear_bit(MT76_HW_SCANNING, &phy->mt76->state)) {
					struct cfg80211_scan_info info = {
						.aborted = false,
					};
					ieee80211_scan_completed(phy->mt76->hw, &info);
				}
				break;
			case UNI_EVENT_SCAN_DONE_CHNLINFO:
				evt = (struct mt7925_mcu_scan_chinfo_event *)tlv->data;

				mt7925_regd_change(phy, evt->alpha2);

				break;
			case UNI_EVENT_SCAN_DONE_NLO:
				ieee80211_sched_scan_results(phy->mt76->hw);
				break;
			default:
				break;
			}

			tlv_len -= le16_to_cpu(tlv->len);
			tlv = (struct tlv *)((char *)(tlv) + le16_to_cpu(tlv->len));
		}

		dev_kfree_skb(skb);
	}
}

static int
mt7925_hw_scan(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
	       struct ieee80211_scan_request *req)
{
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	struct mt76_phy *mphy = hw->priv;
	int err;

	mt792x_mutex_acquire(dev);
	err = mt7925_mcu_hw_scan(mphy, vif, req);
	mt792x_mutex_release(dev);

	return err;
}

static void
mt7925_cancel_hw_scan(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	struct mt76_phy *mphy = hw->priv;

	mt792x_mutex_acquire(dev);
	mt7925_mcu_cancel_hw_scan(mphy, vif);
	mt792x_mutex_release(dev);
}

static int
mt7925_start_sched_scan(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			struct cfg80211_sched_scan_request *req,
			struct ieee80211_scan_ies *ies)
{
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	struct mt76_phy *mphy = hw->priv;
	int err;

	mt792x_mutex_acquire(dev);

	err = mt7925_mcu_sched_scan_req(mphy, vif, req, ies);
	if (err < 0)
		goto out;

	err = mt7925_mcu_sched_scan_enable(mphy, vif, true);
out:
	mt792x_mutex_release(dev);

	return err;
}

static int
mt7925_stop_sched_scan(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	struct mt76_phy *mphy = hw->priv;
	int err;

	mt792x_mutex_acquire(dev);
	err = mt7925_mcu_sched_scan_enable(mphy, vif, false);
	mt792x_mutex_release(dev);

	return err;
}

/* compat: radio_idx added to ieee80211_ops in kernel 6.17 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 17, 0)
static int
mt7925_set_antenna(struct ieee80211_hw *hw, int radio_idx,
		   u32 tx_ant, u32 rx_ant)
#else
static int
mt7925_set_antenna(struct ieee80211_hw *hw, u32 tx_ant, u32 rx_ant)
#endif
{
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	struct mt792x_phy *phy = mt792x_hw_phy(hw);
	int max_nss = hweight8(hw->wiphy->available_antennas_tx);

	if (!tx_ant || tx_ant != rx_ant || ffs(tx_ant) > max_nss)
		return -EINVAL;

	if ((BIT(hweight8(tx_ant)) - 1) != tx_ant)
		tx_ant = BIT(ffs(tx_ant) - 1) - 1;

	mt792x_mutex_acquire(dev);

	phy->mt76->antenna_mask = tx_ant;
	phy->mt76->chainmask = tx_ant;

	mt76_set_stream_caps(phy->mt76, true);
	mt7925_set_stream_he_eht_caps(phy);

	/* TODO: update bmc_wtbl spe_idx when antenna changes */
	mt792x_mutex_release(dev);

	return 0;
}

#ifdef CONFIG_PM
static int mt7925_suspend(struct ieee80211_hw *hw,
			  struct cfg80211_wowlan *wowlan)
{
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	struct mt792x_phy *phy = mt792x_hw_phy(hw);

	cancel_delayed_work_sync(&phy->scan_work);
	cancel_delayed_work_sync(&phy->mt76->mac_work);

	cancel_delayed_work_sync(&dev->pm.ps_work);
	cancel_delayed_work_sync(&dev->mlo_pm_work);
	mt76_connac_free_pending_tx_skbs(&dev->pm, NULL);

	mt792x_mutex_acquire(dev);

	clear_bit(MT76_STATE_RUNNING, &phy->mt76->state);
	ieee80211_iterate_active_interfaces(hw,
					    IEEE80211_IFACE_ITER_RESUME_ALL,
					    mt7925_mcu_set_suspend_iter,
					    &dev->mphy);

	mt792x_mutex_release(dev);

	return 0;
}

static int mt7925_resume(struct ieee80211_hw *hw)
{
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	struct mt792x_phy *phy = mt792x_hw_phy(hw);

	mt792x_mutex_acquire(dev);

	set_bit(MT76_STATE_RUNNING, &phy->mt76->state);
	ieee80211_iterate_active_interfaces(hw,
					    IEEE80211_IFACE_ITER_RESUME_ALL,
					    mt7925_mcu_set_suspend_iter,
					    &dev->mphy);

	ieee80211_queue_delayed_work(hw, &phy->mt76->mac_work,
				     MT792x_WATCHDOG_TIME);

	mt792x_mutex_release(dev);

	return 0;
}

static void mt7925_set_rekey_data(struct ieee80211_hw *hw,
				  struct ieee80211_vif *vif,
				  struct cfg80211_gtk_rekey_data *data)
{
	struct mt792x_dev *dev = mt792x_hw_dev(hw);

	mt792x_mutex_acquire(dev);
	mt76_connac_mcu_update_gtk_rekey(hw, vif, data);
	mt792x_mutex_release(dev);
}
#endif /* CONFIG_PM */

static void mt7925_sta_set_decap_offload(struct ieee80211_hw *hw,
					 struct ieee80211_vif *vif,
					 struct ieee80211_sta *sta,
					 bool enabled)
{
	struct mt792x_sta *msta = (struct mt792x_sta *)sta->drv_priv;
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	unsigned long valid = mvif->valid_links;
	u8 i;

	if (!msta->vif)
		return;

	mt792x_mutex_acquire(dev);

	valid = ieee80211_vif_is_mld(vif) ? mvif->valid_links : BIT(0);

	for_each_set_bit(i, &valid, IEEE80211_MLD_MAX_NUM_LINKS) {
		struct mt792x_bss_conf *mconf;
		struct mt792x_link_sta *mlink;

		mconf = mt792x_vif_to_link(mvif, i);
		mlink = mt792x_sta_to_link(msta, i);

		if (!mlink)
			continue;

		if (enabled)
			set_bit(MT_WCID_FLAG_HDR_TRANS, &mlink->wcid.flags);
		else
			clear_bit(MT_WCID_FLAG_HDR_TRANS, &mlink->wcid.flags);

		if (!mlink->wcid.sta)
			continue;

		mt7925_mcu_wtbl_update_hdr_trans(dev, vif, mconf, mlink);
	}

	mt792x_mutex_release(dev);
}

#if IS_ENABLED(CONFIG_IPV6)
static void __mt7925_ipv6_addr_change(struct ieee80211_hw *hw,
				      struct ieee80211_bss_conf *link_conf,
				      struct inet6_dev *idev)
{
	struct mt792x_bss_conf *mconf = mt792x_link_conf_to_mconf(link_conf);
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	struct inet6_ifaddr *ifa;
	struct sk_buff *skb;
	u8 idx = 0;

	struct {
		struct {
			u8 bss_idx;
			u8 pad[3];
		} __packed hdr;
		struct mt7925_arpns_tlv arpns;
		struct in6_addr ns_addrs[IEEE80211_BSS_ARP_ADDR_LIST_LEN];
	} req_hdr = {
		.hdr = {
			.bss_idx = mconf->mt76.idx,
		},
		.arpns = {
			.tag = cpu_to_le16(UNI_OFFLOAD_OFFLOAD_ND),
			.len = cpu_to_le16(sizeof(req_hdr) - 4),
			.enable = true,
		},
	};

	read_lock_bh(&idev->lock);
	list_for_each_entry(ifa, &idev->addr_list, if_list) {
		if (ifa->flags & IFA_F_TENTATIVE)
			continue;
		req_hdr.ns_addrs[idx] = ifa->addr;
		if (++idx >= IEEE80211_BSS_ARP_ADDR_LIST_LEN)
			break;
	}
	read_unlock_bh(&idev->lock);

	if (!idx)
		return;

	req_hdr.arpns.ips_num = idx;

	skb = __mt76_mcu_msg_alloc(&dev->mt76, NULL, sizeof(req_hdr),
				   0, GFP_ATOMIC);
	if (!skb)
		return;

	skb_put_data(skb, &req_hdr, sizeof(req_hdr));

	skb_queue_tail(&dev->ipv6_ns_list, skb);

	ieee80211_queue_work(dev->mt76.hw, &dev->ipv6_ns_work);
}

static void mt7925_ipv6_addr_change(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif,
				    struct inet6_dev *idev)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	unsigned long valid = ieee80211_vif_is_mld(vif) ?
			      mvif->valid_links : BIT(0);
	struct ieee80211_bss_conf *bss_conf;
	int i;

	for_each_set_bit(i, &valid, IEEE80211_MLD_MAX_NUM_LINKS) {
		bss_conf = mt792x_vif_to_bss_conf(vif, i);
		__mt7925_ipv6_addr_change(hw, bss_conf, idev);
	}
}

#endif

int mt7925_set_tx_sar_pwr(struct ieee80211_hw *hw,
			  const struct cfg80211_sar_specs *sar)
{
	struct mt76_phy *mphy = hw->priv;

	if (sar) {
		int err = mt76_init_sar_power(hw, sar);

		if (err)
			return err;
	}
	mt792x_init_acpi_sar_power(mt792x_hw_phy(hw), !sar);

	return mt7925_mcu_set_rate_txpower(mphy);
}

static int mt7925_set_sar_specs(struct ieee80211_hw *hw,
				const struct cfg80211_sar_specs *sar)
{
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	int err;

	mt792x_mutex_acquire(dev);
	err = mt7925_mcu_set_clc(dev, dev->mt76.alpha2,
			 dev->country_ie_env);
	if (err < 0)
		goto out;

	err = mt7925_set_tx_sar_pwr(hw, sar);

out:
	mt792x_mutex_release(dev);
	return err;
}

static void
mt7925_channel_switch_beacon(struct ieee80211_hw *hw,
			     struct ieee80211_vif *vif,
			     struct cfg80211_chan_def *chandef)
{
	struct mt792x_dev *dev = mt792x_hw_dev(hw);

	mt792x_mutex_acquire(dev);
	mt7925_mcu_uni_add_beacon_offload(dev, hw, vif, true);
	mt792x_mutex_release(dev);
}

static int
mt7925_conf_tx(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
	       unsigned int link_id, u16 queue,
	       const struct ieee80211_tx_queue_params *params)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct mt792x_bss_conf *mconf = mt792x_vif_to_link(mvif, link_id);
	static const u8 mq_to_aci[] = {
		    [IEEE80211_AC_VO] = 3,
		    [IEEE80211_AC_VI] = 2,
		    [IEEE80211_AC_BE] = 0,
		    [IEEE80211_AC_BK] = 1,
	};

	/* firmware uses access class index */
	mconf->queue_params[mq_to_aci[queue]] = *params;

	return 0;
}

static int
mt7925_start_ap(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		struct ieee80211_bss_conf *link_conf)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	int err;

	mt792x_mutex_acquire(dev);

	err = mt7925_mcu_add_bss_info(&dev->phy, mvif->bss_conf.mt76.ctx,
				      link_conf, NULL, true);
	if (err)
		goto out;

	err = mt7925_mcu_set_bss_pm(dev, link_conf, true);
	if (err)
		goto out;

	err = mt7925_mcu_sta_update(dev, NULL, vif,
				    &mvif->sta.deflink, true,
				    MT76_STA_INFO_STATE_NONE);
out:
	mt792x_mutex_release(dev);

	return err;
}

static void
mt7925_stop_ap(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
	       struct ieee80211_bss_conf *link_conf)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	int err;

	mt792x_mutex_acquire(dev);

	err = mt7925_mcu_set_bss_pm(dev, link_conf, false);
	if (err)
		goto out;

	mt7925_mcu_add_bss_info(&dev->phy, mvif->bss_conf.mt76.ctx, link_conf,
				NULL, false);

out:
	mt792x_mutex_release(dev);
}

static int
mt7925_add_chanctx(struct ieee80211_hw *hw,
		   struct ieee80211_chanctx_conf *ctx)
{
	struct mt792x_dev *dev = mt792x_hw_dev(hw);

	dev->new_ctx = ctx;

	return 0;
}

static void
mt7925_remove_chanctx(struct ieee80211_hw *hw,
		      struct ieee80211_chanctx_conf *ctx)
{
	struct mt792x_dev *dev = mt792x_hw_dev(hw);

	if (dev->new_ctx == ctx)
		dev->new_ctx = NULL;

}

static void
mt7925_change_chanctx(struct ieee80211_hw *hw,
		      struct ieee80211_chanctx_conf *ctx,
		      u32 changed)
{
	struct mt792x_chanctx *mctx = (struct mt792x_chanctx *)ctx->drv_priv;
	struct mt792x_phy *phy = mt792x_hw_phy(hw);
	struct mt792x_bss_conf *mconf;
	struct ieee80211_vif *vif;
	struct mt792x_vif *mvif;

	if (!mctx->bss_conf)
		return;

	mconf = mctx->bss_conf;
	mvif = mconf->vif;
	vif = container_of((void *)mvif, struct ieee80211_vif, drv_priv);

	mt792x_mutex_acquire(phy->dev);
	if (vif->type == NL80211_IFTYPE_MONITOR) {
		mt7925_monitor_arm_sniffer(phy, vif, ctx);
	} else {
		if (ieee80211_vif_is_mld(vif)) {
			unsigned long valid = mvif->valid_links;
			u8 i;

			for_each_set_bit(i, &valid, IEEE80211_MLD_MAX_NUM_LINKS) {
				mconf = mt792x_vif_to_link(mvif, i);
				if (mconf && mconf->mt76.ctx == ctx)
					break;
			}

		} else {
			mconf = &mvif->bss_conf;
		}

		if (mconf) {
			struct ieee80211_bss_conf *link_conf;

			link_conf = mt792x_vif_to_bss_conf(vif, mconf->link_id);
			mt7925_mcu_set_chctx(mvif->phy->mt76, &mconf->mt76,
					     link_conf, ctx);

			if (changed & IEEE80211_CHANCTX_CHANGE_PUNCTURING)
				mt7925_mcu_set_eht_pp(mvif->phy->mt76, &mconf->mt76,
						      link_conf, ctx);
		}
	}

	mt792x_mutex_release(phy->dev);
}

static void mt7925_mgd_prepare_tx(struct ieee80211_hw *hw,
				  struct ieee80211_vif *vif,
				  struct ieee80211_prep_tx_info *info)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	u16 duration = info->duration ? info->duration :
		       jiffies_to_msecs(HZ);

	mt792x_mutex_acquire(dev);
	mt7925_set_roc(mvif->phy, &mvif->bss_conf,
		       mvif->bss_conf.mt76.ctx->def.chan, duration,
		       MT7925_ROC_REQ_JOIN);
	mt792x_mutex_release(dev);
}

static void mt7925_mgd_complete_tx(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif,
				   struct ieee80211_prep_tx_info *info)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;

	mt7925_abort_roc(mvif->phy, &mvif->bss_conf);
}

static void mt7925_vif_cfg_changed(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif,
				   u64 changed)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	unsigned long valid = ieee80211_vif_is_mld(vif) ?
				      mvif->valid_links : BIT(0);
	struct ieee80211_bss_conf *bss_conf;
	int i;

	mt792x_mutex_acquire(dev);

	if (changed & BSS_CHANGED_ASSOC) {
		mt7925_mcu_sta_update(dev, NULL, vif,
				      &mvif->sta.deflink, true,
				      MT76_STA_INFO_STATE_ASSOC);
		mt7925_mcu_set_beacon_filter(dev, vif, vif->cfg.assoc);

		if (ieee80211_vif_is_mld(vif))
			mvif->mlo_pm_state = MT792x_MLO_LINK_ASSOC;

		mt7925_mlo_update_rssi_monitor(dev, vif);
	}

	if (changed & BSS_CHANGED_ARP_FILTER) {
		for_each_set_bit(i, &valid, IEEE80211_MLD_MAX_NUM_LINKS) {
			bss_conf = mt792x_vif_to_bss_conf(vif, i);
			mt7925_mcu_update_arp_filter(&dev->mt76, bss_conf);
		}
	}

	if (changed & BSS_CHANGED_PS) {
		if (hweight16(mvif->valid_links) < 2) {
			/* legacy */
			bss_conf = &vif->bss_conf;
			mt7925_mcu_uni_bss_ps(dev, bss_conf);
		} else {
			if (mvif->mlo_pm_state == MT792x_MLO_LINK_ASSOC) {
				mvif->mlo_pm_state = MT792x_MLO_CHANGED_PS_PENDING;
			} else if (mvif->mlo_pm_state == MT792x_MLO_CHANGED_PS) {
				for_each_set_bit(i, &valid, IEEE80211_MLD_MAX_NUM_LINKS) {
					bss_conf = mt792x_vif_to_bss_conf(vif, i);
					mt7925_mcu_uni_bss_ps(dev, bss_conf);
				}
			}
		}
	}

	mt792x_mutex_release(dev);
}

static void mt7925_link_info_changed(struct ieee80211_hw *hw,
				     struct ieee80211_vif *vif,
				     struct ieee80211_bss_conf *info,
				     u64 changed)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct mt792x_phy *phy = mt792x_hw_phy(hw);
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	struct mt792x_bss_conf *mconf;

	mconf = mt792x_vif_to_link(mvif, info->link_id);

	mt792x_mutex_acquire(dev);

	if (changed & BSS_CHANGED_ERP_SLOT) {
		int slottime = info->use_short_slot ? 9 : 20;

		if (slottime != phy->slottime) {
			phy->slottime = slottime;
			mt7925_mcu_set_timing(phy, info);
		}
	}

	if (changed & BSS_CHANGED_MCAST_RATE)
		mconf->mt76.mcast_rates_idx =
				mt7925_get_rates_table(hw, vif, false, true);

	if (changed & BSS_CHANGED_BASIC_RATES)
		mconf->mt76.basic_rates_idx =
				mt7925_get_rates_table(hw, vif, false, false);

	if (changed & (BSS_CHANGED_BEACON |
		       BSS_CHANGED_BEACON_ENABLED)) {
		mconf->mt76.beacon_rates_idx =
				mt7925_get_rates_table(hw, vif, true, false);

		mt7925_mcu_uni_add_beacon_offload(dev, hw, vif,
						  info->enable_beacon);
	}

	/* ensure that enable txcmd_mode after bss_info */
	if (changed & (BSS_CHANGED_QOS | BSS_CHANGED_BEACON_ENABLED))
		mt7925_mcu_set_tx(dev, info);

	if (mvif->mlo_pm_state == MT792x_MLO_CHANGED_PS_PENDING) {
		/* Indicate the secondary setup done */
		mt7925_mcu_uni_bss_bcnft(dev, info, true);

		ieee80211_queue_delayed_work(hw, &dev->mlo_pm_work, 5 * HZ);
		mvif->mlo_pm_state = MT792x_MLO_CHANGED_PS;
	}

	if (changed & BSS_CHANGED_CQM)
		mt7925_mcu_set_rssimonitor(dev, vif);

	mt792x_mutex_release(dev);
}

static int
mt7925_change_vif_links(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			u16 old_links, u16 new_links,
			struct ieee80211_bss_conf *old[IEEE80211_MLD_MAX_NUM_LINKS])
{
	struct mt792x_bss_conf *mconfs[IEEE80211_MLD_MAX_NUM_LINKS] = {}, *mconf;
	struct mt792x_link_sta *mlinks[IEEE80211_MLD_MAX_NUM_LINKS] = {}, *mlink;
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	unsigned long add = new_links & ~old_links;
	unsigned long rem = old_links & ~new_links;
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	struct mt792x_phy *phy = mt792x_hw_phy(hw);
	struct ieee80211_bss_conf *link_conf;
	unsigned long bss_added = 0;
	unsigned int link_id;
	int err;

	if (old_links == new_links)
		return 0;

	if (READ_ONCE(mt76_mlo_diag_trace))
		printk(KERN_INFO
		      "MLO_LINK_ADD_START: old_links=0x%x new_links=0x%x add=0x%lx rem=0x%lx\n",
		      old_links, new_links, add, rem);

	mt792x_mutex_acquire(dev);

	/* Any MLO JOIN reservation must be handed back before the BSSes it
	 * refers to are destroyed, otherwise the firmware keeps it forever
	 * and never grants another one.
	 */
	if (rem && phy->mlo_roc_token_id && (mt7925_mlo_roc_release & 2)) {
		if (READ_ONCE(mt76_mlo_diag_trace))
			printk(KERN_INFO "ROC_ABORT: site=change_vif_links_teardown token=%u dbdcband=0xfe\n",
			       phy->mlo_roc_token_id);
		mt7925_mcu_abort_mlo_roc(phy, &mvif->bss_conf,
					 phy->mlo_roc_token_id);
		phy->mlo_roc_token_id = 0;
		clear_bit(MT76_STATE_ROC, &phy->mt76->state);
	}

	for_each_set_bit(link_id, &rem, IEEE80211_MLD_MAX_NUM_LINKS) {
		mconf = mt792x_vif_to_link(mvif, link_id);
		mlink = mt792x_sta_to_link(&mvif->sta, link_id);

		if (!mconf || !mlink)
			continue;

		if (mconf != &mvif->bss_conf) {
			mt792x_mac_link_bss_remove(dev, mconf, mlink);
			devm_kfree(dev->mt76.dev, mconf);
			devm_kfree(dev->mt76.dev, mlink);
		}

		rcu_assign_pointer(mvif->link_conf[link_id], NULL);
		rcu_assign_pointer(mvif->sta.link[link_id], NULL);
	}

	for_each_set_bit(link_id, &add, IEEE80211_MLD_MAX_NUM_LINKS) {
		if (!old_links) {
			mvif->deflink_id = link_id;
			mconf = &mvif->bss_conf;
			mlink = &mvif->sta.deflink;
		} else {
			mconf = devm_kzalloc(dev->mt76.dev, sizeof(*mconf),
					     GFP_KERNEL);
			mlink = devm_kzalloc(dev->mt76.dev, sizeof(*mlink),
					     GFP_KERNEL);
			if (!mconf || !mlink) {
				mt792x_mutex_release(dev);
				return -ENOMEM;
			}
		}

		mconfs[link_id] = mconf;
		mlinks[link_id] = mlink;
		mconf->link_id = link_id;
		mconf->vif = mvif;
		mlink->wcid.link_id = link_id;
		mlink->wcid.link_valid = !!vif->valid_links;
		mlink->wcid.def_wcid = &mvif->sta.deflink.wcid;
	}

	if (hweight16(mvif->valid_links) == 0)
		mt792x_mac_link_bss_remove(dev, &mvif->bss_conf,
					   &mvif->sta.deflink);

	for_each_set_bit(link_id, &add, IEEE80211_MLD_MAX_NUM_LINKS) {
		mconf = mconfs[link_id];
		mlink = mlinks[link_id];
		link_conf = mt792x_vif_to_bss_conf(vif, link_id);

		rcu_assign_pointer(mvif->link_conf[link_id], mconf);
		rcu_assign_pointer(mvif->sta.link[link_id], mlink);

		err = mt7925_mac_link_bss_add(dev, link_conf, mlink);
		if (err < 0)
			goto free;

		/* The firmware BSS now exists for this link; it must be torn
		 * down properly if a later step fails.
		 */
		__set_bit(link_id, &bss_added);

		if (mconf != &mvif->bss_conf) {
			dev_info(dev->mt76.dev,
				 "CVL roc link_id=%u add=0x%lx active=0x%x valid=0x%x defl=%u defidx=%u omac=%u vif_mask=0x%llx omac_mask=0x%llx\n",
				 link_id, add, vif->active_links,
				 vif->valid_links, mvif->deflink_id,
				 mvif->bss_conf.mt76.idx,
				 mvif->bss_conf.mt76.omac_idx,
				 dev->mt76.vif_mask, phy->omac_mask);

			err = mt7925_set_mlo_roc(phy, &mvif->bss_conf,
						 vif->active_links);
			if (err < 0)
				goto free;
		}
	}

	mvif->valid_links = new_links;

	mt792x_mutex_release(dev);

	return 0;

free:
	for_each_set_bit(link_id, &add, IEEE80211_MLD_MAX_NUM_LINKS) {
		/* Links whose mt7925_mac_link_bss_add() succeeded own a
		 * firmware BSS plus host state (vif_mask/omac_mask/WCID) and
		 * need the full teardown. Links that failed inside
		 * mt7925_mac_link_bss_add() already unwound their own host
		 * state there and must not be torn down again.
		 */
		if (test_bit(link_id, &bss_added))
			mt792x_mac_link_bss_remove(dev, mconfs[link_id],
						   mlinks[link_id]);

		rcu_assign_pointer(mvif->link_conf[link_id], NULL);
		rcu_assign_pointer(mvif->sta.link[link_id], NULL);

		if (mconfs[link_id] != &mvif->bss_conf)
			devm_kfree(dev->mt76.dev, mconfs[link_id]);
		if (mlinks[link_id] != &mvif->sta.deflink)
			devm_kfree(dev->mt76.dev, mlinks[link_id]);
	}

	mt792x_mutex_release(dev);

	return err;
}

static int
mt7925_change_sta_links(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			struct ieee80211_sta *sta, u16 old_links, u16 new_links)
{
	unsigned long add = new_links & ~old_links;
	unsigned long rem = old_links & ~new_links;
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	int err = 0;

	dev_info(dev->mt76.dev,
		 "EXPERIMENTAL MLO TEST: change_sta_links old=0x%x new=0x%x add=0x%lx rem=0x%lx\n",
		 old_links, new_links, add, rem);

	if (old_links == new_links)
		return 0;

	mt7925_mlo_clear_tid_link((struct mt792x_sta *)sta->drv_priv, -1);

	mt792x_mutex_acquire(dev);

	err = mt7925_mac_sta_remove_links(dev, vif, sta, rem);
	if (err < 0)
		goto out;

	err = mt7925_mac_sta_add_links(dev, vif, sta, add);
	if (err < 0)
		goto out;

out:
	mt792x_mutex_release(dev);

	return err;
}

static int
mt7927_reconfig_band(struct mt792x_dev *dev, struct ieee80211_vif *vif,
		     struct ieee80211_bss_conf *link_conf,
		     struct mt792x_bss_conf *mconf,
		     u8 band_idx)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct mt792x_link_sta *mlink = &mvif->sta.deflink;
	int ret;

	ret = mt76_connac_mcu_uni_add_dev(&dev->mphy, link_conf,
					  &mconf->mt76, &mlink->wcid,
					  false);
	if (ret)
		return ret;

	mconf->mt76.band_idx = band_idx;

	return mt76_connac_mcu_uni_add_dev(&dev->mphy, link_conf,
					   &mconf->mt76, &mlink->wcid,
					   true);
}

static int mt7925_assign_vif_chanctx(struct ieee80211_hw *hw,
				     struct ieee80211_vif *vif,
				     struct ieee80211_bss_conf *link_conf,
				     struct ieee80211_chanctx_conf *ctx)
{
	struct mt792x_chanctx *mctx = (struct mt792x_chanctx *)ctx->drv_priv;
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	struct ieee80211_bss_conf *pri_link_conf;
	struct mt792x_bss_conf *mconf;
	u8 band_idx;

	mutex_lock(&dev->mt76.mutex);

	if (ieee80211_vif_is_mld(vif)) {
		mconf = mt792x_vif_to_link(mvif, link_conf->link_id);
		pri_link_conf = mt792x_vif_to_bss_conf(vif, mvif->deflink_id);

		if (vif->type == NL80211_IFTYPE_STATION &&
		    mconf == &mvif->bss_conf)
			mt7925_mcu_add_bss_info(&dev->phy, NULL, pri_link_conf,
						NULL, true);
	} else {
		mconf = &mvif->bss_conf;

		if (is_mt7927(&dev->mt76)) {
			band_idx = mt7927_band_idx(ctx->def.chan->band);

			mt7927_reconfig_band(dev, vif, link_conf, mconf, band_idx);
			mconf->mt76.band_idx = band_idx;
			mvif->sta.deflink.wcid.phy_idx = band_idx;
		}
	}

	/* A monitor vif gets a fresh chanctx on every channel/band change;
	 * re-arm the sniffer here so it actually retunes instead of staying
	 * stuck on the band where monitor was first enabled. Do this before
	 * mconf->mt76.ctx is pointed at the new ctx below: the helper needs
	 * to see the still-old ctx (or NULL, on first assignment) to detect
	 * a cross-band move correctly; it sets mconf->mt76.ctx itself once
	 * the old-band teardown is done.
	 */
	if (vif->type == NL80211_IFTYPE_MONITOR) {
		mt7925_monitor_arm_sniffer(mvif->phy, vif, ctx);
	} else {
		mconf->mt76.ctx = ctx;
	}
	mctx->bss_conf = mconf;

	mutex_unlock(&dev->mt76.mutex);

	return 0;
}

static void mt7925_unassign_vif_chanctx(struct ieee80211_hw *hw,
					struct ieee80211_vif *vif,
					struct ieee80211_bss_conf *link_conf,
					struct ieee80211_chanctx_conf *ctx)
{
	struct mt792x_chanctx *mctx = (struct mt792x_chanctx *)ctx->drv_priv;
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	struct mt792x_bss_conf *mconf;

	mutex_lock(&dev->mt76.mutex);

	if (ieee80211_vif_is_mld(vif)) {
		mconf = mt792x_vif_to_link(mvif, link_conf->link_id);

		if (vif->type == NL80211_IFTYPE_STATION &&
		    mconf == &mvif->bss_conf)
			mt7925_mcu_add_bss_info(&dev->phy, NULL, link_conf,
						NULL, false);
	} else {
		mconf = &mvif->bss_conf;
	}

	mctx->bss_conf = NULL;
	mconf->mt76.ctx = NULL;
	mutex_unlock(&dev->mt76.mutex);

	if (link_conf->csa_active) {
		timer_delete_sync(&mvif->csa_timer);
		cancel_work_sync(&mvif->csa_work);
	}
}

static void mt7925_rfkill_poll(struct ieee80211_hw *hw)
{
	struct mt792x_phy *phy = mt792x_hw_phy(hw);
	int ret;

	mt792x_mutex_acquire(phy->dev);
	ret = mt7925_mcu_wf_rf_pin_ctrl(phy);
	mt792x_mutex_release(phy->dev);

	wiphy_rfkill_set_hw_state(hw->wiphy, ret == 0);
}

static int mt7925_switch_vif_chanctx(struct ieee80211_hw *hw,
				     struct ieee80211_vif_chanctx_switch *vifs,
				     int n_vifs,
				     enum ieee80211_chanctx_switch_mode mode)
{
	return mt7925_assign_vif_chanctx(hw, vifs->vif, vifs->link_conf,
					 vifs->new_ctx);
}

void mt7925_csa_work(struct work_struct *work)
{
	struct mt792x_vif *mvif;
	struct mt792x_dev *dev;
	struct ieee80211_vif *vif;
	struct ieee80211_bss_conf *link_conf;
	struct mt792x_bss_conf *mconf;
	u8 link_id, roc_rtype;
	int ret = 0;

	mvif = (struct mt792x_vif *)container_of(work, struct mt792x_vif,
						csa_work);
	dev = mvif->phy->dev;
	vif = container_of((void *)mvif, struct ieee80211_vif, drv_priv);

	if (ieee80211_vif_is_mld(vif))
		return;

	if (!dev->new_ctx)
		return;

	link_id = 0;
	mconf = &mvif->bss_conf;
	link_conf = &vif->bss_conf;
	roc_rtype = MT7925_ROC_REQ_JOIN;

	mt792x_mutex_acquire(dev);
	ret = mt7925_set_roc(mvif->phy, mconf, dev->new_ctx->def.chan,
			     4000, roc_rtype);
	mt792x_mutex_release(dev);
	if (!ret) {
		mt792x_mutex_acquire(dev);
		ret = mt7925_mcu_set_chctx(mvif->phy->mt76, &mconf->mt76, link_conf,
					   dev->new_ctx);
		mt792x_mutex_release(dev);

		mt7925_abort_roc(mvif->phy, mconf);
	}

	ieee80211_chswitch_done(vif, !ret, link_id);
}

static int mt7925_pre_channel_switch(struct ieee80211_hw *hw,
				     struct ieee80211_vif *vif,
				     struct ieee80211_channel_switch *chsw)
{
	if (ieee80211_vif_is_mld(vif))
		return -EOPNOTSUPP;

	if (vif->type != NL80211_IFTYPE_STATION || !vif->cfg.assoc)
		return -EOPNOTSUPP;

	if (!cfg80211_chandef_usable(hw->wiphy, &chsw->chandef,
				     IEEE80211_CHAN_DISABLED))
		return -EOPNOTSUPP;

	return 0;
}

static void mt7925_channel_switch(struct ieee80211_hw *hw,
				  struct ieee80211_vif *vif,
				  struct ieee80211_channel_switch *chsw)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	u16 beacon_interval;

	if (ieee80211_vif_is_mld(vif))
		return;

	beacon_interval = vif->bss_conf.beacon_int;

	mvif->csa_timer.expires = TU_TO_EXP_TIME(beacon_interval * chsw->count);
	add_timer(&mvif->csa_timer);
}

static void mt7925_abort_channel_switch(struct ieee80211_hw *hw,
					struct ieee80211_vif *vif,
					struct ieee80211_bss_conf *link_conf)
{
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;

	timer_delete_sync(&mvif->csa_timer);
	cancel_work_sync(&mvif->csa_work);
}

static void mt7925_channel_switch_rx_beacon(struct ieee80211_hw *hw,
					    struct ieee80211_vif *vif,
					    struct ieee80211_channel_switch *chsw)
{
	struct mt792x_dev *dev = mt792x_hw_dev(hw);
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	u16 beacon_interval;

	if (ieee80211_vif_is_mld(vif))
		return;

	if (!dev->new_ctx)
		return;

	beacon_interval = vif->bss_conf.beacon_int;

	if (cfg80211_chandef_identical(&chsw->chandef,
				       &dev->new_ctx->def) &&
				       chsw->count) {
		mod_timer(&mvif->csa_timer,
			  TU_TO_EXP_TIME(beacon_interval * chsw->count));
	}
}

static void mt7925_stop(struct ieee80211_hw *hw, bool suspend)
{
	struct mt792x_dev *dev = mt792x_hw_dev(hw);

	cancel_delayed_work_sync(&dev->mlo_pm_work);

	mt792x_stop(hw, suspend);
}

static void mt7925_sta_pre_rcu_remove(struct ieee80211_hw *hw,
				      struct ieee80211_vif *vif,
				      struct ieee80211_sta *sta)
{
	struct mt76_phy *phy = hw->priv;
	struct mt76_dev *dev = phy->dev;
	struct mt76_wcid *wcid = (struct mt76_wcid *)sta->drv_priv;

	mutex_lock(&dev->mutex);
	spin_lock_bh(&dev->status_lock);

	if (ieee80211_vif_is_mld(vif)) {
		struct mt792x_sta *msta = (struct mt792x_sta *)sta->drv_priv;
		struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
		unsigned long valid = mvif->valid_links;
		struct mt792x_link_sta *mlink;
		unsigned int link_id;

		for_each_set_bit(link_id, &valid, IEEE80211_MLD_MAX_NUM_LINKS) {
			mlink = mt792x_sta_to_link(msta, link_id);
			if (!mlink || !mlink->wcid.sta)
				continue;
			if (mlink->wcid.idx < ARRAY_SIZE(dev->wcid))
				rcu_assign_pointer(dev->wcid[mlink->wcid.idx],
						   NULL);
		}
	} else {
		rcu_assign_pointer(dev->wcid[wcid->idx], NULL);
	}

	spin_unlock_bh(&dev->status_lock);
	mutex_unlock(&dev->mutex);
}

const struct ieee80211_ops mt7925_ops = {
	.tx = mt792x_tx,
	.start = mt7925_start,
	.stop = mt7925_stop,
	.add_interface = mt7925_add_interface,
	.remove_interface = mt7925_remove_interface,
	.config = mt7925_config,
	.conf_tx = mt7925_conf_tx,
	.configure_filter = mt7925_configure_filter,
	.start_ap = mt7925_start_ap,
	.stop_ap = mt7925_stop_ap,
	.sta_state = mt76_sta_state,
	.sta_pre_rcu_remove = mt7925_sta_pre_rcu_remove,
	.set_key = mt7925_set_key,
	.sta_set_decap_offload = mt7925_sta_set_decap_offload,
#if IS_ENABLED(CONFIG_IPV6)
	.ipv6_addr_change = mt7925_ipv6_addr_change,
#endif /* CONFIG_IPV6 */
	.ampdu_action = mt7925_ampdu_action,
	.set_rts_threshold = mt7925_set_rts_threshold,
	.wake_tx_queue = mt76_wake_tx_queue,
	.release_buffered_frames = mt76_release_buffered_frames,
	.channel_switch_beacon = mt7925_channel_switch_beacon,
	.get_txpower = mt792x_get_txpower,
	.get_stats = mt792x_get_stats,
	.get_et_sset_count = mt792x_get_et_sset_count,
	.get_et_strings = mt792x_get_et_strings,
	.get_et_stats = mt792x_get_et_stats,
	.get_tsf = mt792x_get_tsf,
	.set_tsf = mt792x_set_tsf,
	.get_survey = mt76_get_survey,
	.get_antenna = mt76_get_antenna,
	.set_antenna = mt7925_set_antenna,
	.set_coverage_class = mt792x_set_coverage_class,
	.hw_scan = mt7925_hw_scan,
	.cancel_hw_scan = mt7925_cancel_hw_scan,
	.sta_statistics = mt792x_sta_statistics,
	.sched_scan_start = mt7925_start_sched_scan,
	.sched_scan_stop = mt7925_stop_sched_scan,
	CFG80211_TESTMODE_CMD(mt7925_testmode_cmd)
	CFG80211_TESTMODE_DUMP(mt7925_testmode_dump)
#ifdef CONFIG_PM
	.suspend = mt7925_suspend,
	.resume = mt7925_resume,
	.set_wakeup = mt792x_set_wakeup,
	.set_rekey_data = mt7925_set_rekey_data,
#endif /* CONFIG_PM */
	.flush = mt792x_flush,
	.set_sar_specs = mt7925_set_sar_specs,
	.remain_on_channel = mt7925_remain_on_channel,
	.cancel_remain_on_channel = mt7925_cancel_remain_on_channel,
	.add_chanctx = mt7925_add_chanctx,
	.remove_chanctx = mt7925_remove_chanctx,
	.change_chanctx = mt7925_change_chanctx,
	.assign_vif_chanctx = mt7925_assign_vif_chanctx,
	.unassign_vif_chanctx = mt7925_unassign_vif_chanctx,
	.mgd_prepare_tx = mt7925_mgd_prepare_tx,
	.mgd_complete_tx = mt7925_mgd_complete_tx,
	.vif_cfg_changed = mt7925_vif_cfg_changed,
	.link_info_changed = mt7925_link_info_changed,
	.change_vif_links = mt7925_change_vif_links,
	.change_sta_links = mt7925_change_sta_links,
	.can_neg_ttlm = mt7925_mlo_can_neg_ttlm,
	.rfkill_poll = mt7925_rfkill_poll,

	.switch_vif_chanctx = mt7925_switch_vif_chanctx,
	.pre_channel_switch = mt7925_pre_channel_switch,
	.channel_switch = mt7925_channel_switch,
	.abort_channel_switch = mt7925_abort_channel_switch,
	.channel_switch_rx_beacon = mt7925_channel_switch_rx_beacon,
};
EXPORT_SYMBOL_GPL(mt7925_ops);

MODULE_AUTHOR("Deren Wu <deren.wu@mediatek.com>");
MODULE_DESCRIPTION("MediaTek MT7925 core driver");
MODULE_LICENSE("Dual BSD/GPL");

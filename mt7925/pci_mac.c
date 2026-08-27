// SPDX-License-Identifier: BSD-3-Clause-Clear
/* Copyright (C) 2023 MediaTek Inc. */

#include "mt7925.h"
#include "../dma.h"
#include "mcu.h"
#include "mac.h"
#include "../trace.h"

int mt7925e_tx_prepare_skb(struct mt76_dev *mdev, void *txwi_ptr,
			   enum mt76_txq_id qid, struct mt76_wcid *wcid,
			   struct ieee80211_sta *sta,
			   struct mt76_tx_info *tx_info)
{
	struct mt792x_dev *dev = container_of(mdev, struct mt792x_dev, mt76);
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(tx_info->skb);
	struct ieee80211_key_conf *key = info->control.hw_key;
	struct mt76_connac_hw_txp *txp;
	struct mt76_txwi_cache *t;
	int id, pid;
	u8 *txwi = (u8 *)txwi_ptr;

	if (unlikely(tx_info->skb->len <= ETH_HLEN))
		return -EINVAL;

	if (!wcid)
		wcid = &dev->mt76.global_wcid;

	t = (struct mt76_txwi_cache *)(txwi + mdev->drv->txwi_size);
	t->skb = tx_info->skb;

	id = mt76_token_consume(mdev, &t);
	if (id < 0)
		return id;

	if (sta) {
		struct mt792x_sta *msta = (struct mt792x_sta *)sta->drv_priv;

		if (time_after(jiffies, msta->deflink.last_txs + HZ / 4)) {
			info->flags |= IEEE80211_TX_CTL_REQ_TX_STATUS;
			msta->deflink.last_txs = jiffies;
		}
	}

	pid = mt76_tx_status_skb_add(mdev, wcid, tx_info->skb);
	t->mlo_diag_eapol = unlikely(mt76_mlo_diag_trace) &&
			    tx_info->skb->protocol == cpu_to_be16(ETH_P_PAE);
	t->mlo_diag_token = id;
	t->mlo_diag_pid = pid;
	mt7925_mac_write_txwi(mdev, txwi_ptr, tx_info->skb, wcid, key,
			      pid, qid, 0);
	if (unlikely(mt76_mlo_diag_trace)) {
		struct mt792x_bss_conf *mconf = NULL;
		struct mt76_phy *dma_phy = mt76_dev_phy(mdev, t->phy_idx);
		struct mt76_queue *q = dma_phy ? dma_phy->q_tx[t->qid] : NULL;
		struct mt792x_link_sta *mlink;
		struct mt76_mlo_txwi_trace v = {};
		int mconf_link = -1, bss = -1, wmm = -1;
		u32 txd0 = le32_to_cpu(((__le32 *)txwi_ptr)[0]);
		u32 txd1 = le32_to_cpu(((__le32 *)txwi_ptr)[1]);
		u32 txd3 = le32_to_cpu(((__le32 *)txwi_ptr)[3]);

		if (wcid->sta) {
			mlink = container_of(wcid, struct mt792x_link_sta, wcid);
			if (mlink->sta && mlink->sta->vif) {
				rcu_read_lock();
				mconf = rcu_dereference(mlink->sta->vif->link_conf[wcid->link_id]);
				if (mconf) {
					mconf_link = mconf->link_id;
					bss = mconf->mt76.idx;
					wmm = mconf->mt76.wmm_idx;
				}
				rcu_read_unlock();
			}
		}
		v = (struct mt76_mlo_txwi_trace) {
			.token = id, .pid = pid, .wcid = wcid->idx,
			.wlan = FIELD_GET(MT_TXD1_WLAN_IDX, txd1),
			.wcid_cipher = wcid->cipher,
			.seq = FIELD_GET(MT_TXD3_SEQ, txd3),
			.mconf_link = mconf_link, .bss = bss,
			.omac = FIELD_GET(MT_TXD1_OWN_MAC, txd1), .wmm = wmm,
			.key_idx = key ? key->keyidx : -1,
			.hw_key_idx = wcid->hw_key_idx, .ring = q ? q->hw_idx : -1,
			.key_cipher = key ? key->cipher : 0,
			.link = wcid->link_id, .sel_phy = wcid->phy_idx,
			.txd_qidx = FIELD_GET(MT_TXD0_Q_IDX, txd0),
			.tgid = FIELD_GET(MT_TXD1_TGID, txd1),
			.tid = FIELD_GET(MT_TXD1_TID, txd1), .qid = qid,
			.dma_phy = t->phy_idx,
			.protect = !!(txd3 & MT_TXD3_PROTECT_FRAME),
			.ba_disable = !!(txd3 & MT_TXD3_BA_DISABLE),
			.hw_amsdu = !!(txd3 & MT_TXD3_HW_AMSDU),
			.sn_valid = !!(txd3 & MT_TXD3_SN_VALID),
		};
		trace_mlo_txwi(mdev, &v);
	}

	txp = (struct mt76_connac_hw_txp *)(txwi + MT_TXD_SIZE);
	memset(txp, 0, sizeof(struct mt76_connac_hw_txp));
	mt76_connac_write_hw_txp(mdev, tx_info, txp, id);

	if (t->mlo_diag_eapol) {
		printk(KERN_INFO
		       "MLO_EAPOL_TXWI_FINAL: token=%d pid=%d wcid=%u qid=%u phy=%u txwi_dma=%pad len=%u\n",
		       id, pid, wcid->idx, qid, t->phy_idx, &t->dma_addr,
		       mdev->drv->txwi_size);
		print_hex_dump(KERN_INFO, "MLO_EAPOL_TXWI_FINAL: ",
			       DUMP_PREFIX_OFFSET, 16, 1, txwi_ptr,
			       mdev->drv->txwi_size, false);
	}

	tx_info->skb = NULL;

	return 0;
}

void mt7925_tx_token_put(struct mt792x_dev *dev)
{
	struct mt76_txwi_cache *txwi;
	int id;

	spin_lock_bh(&dev->mt76.token_lock);
	idr_for_each_entry(&dev->mt76.token, txwi, id) {
		mt7925_txwi_free(dev, txwi, NULL, NULL, NULL);
		dev->mt76.token_count--;
	}
	spin_unlock_bh(&dev->mt76.token_lock);
	idr_destroy(&dev->mt76.token);
}

int mt7925e_mac_reset(struct mt792x_dev *dev)
{
	const struct mt792x_irq_map *irq_map = dev->irq_map;
	int i, err;

	mt792xe_mcu_drv_pmctrl(dev);

	mt76_connac_free_pending_tx_skbs(&dev->pm, NULL);

	mt76_wr(dev, dev->irq_map->host_irq_enable, 0);
	mt76_wr(dev, dev->pcie_reg->imask, 0x0);

	set_bit(MT76_RESET, &dev->mphy.state);
	set_bit(MT76_MCU_RESET, &dev->mphy.state);
	wake_up(&dev->mt76.mcu.wait);
	skb_queue_purge(&dev->mt76.mcu.res_q);

	mt76_txq_schedule_all(&dev->mphy);

	mt76_worker_disable(&dev->mt76.tx_worker);
	if (irq_map->rx.data_complete_mask)
		napi_disable(&dev->mt76.napi[MT_RXQ_MAIN]);
	if (irq_map->rx.wm_complete_mask)
		napi_disable(&dev->mt76.napi[MT_RXQ_MCU]);
	if (irq_map->rx.wm2_complete_mask)
		napi_disable(&dev->mt76.napi[MT_RXQ_MCU_WA]);
	if (irq_map->tx.all_complete_mask)
		napi_disable(&dev->mt76.tx_napi);

	mt7925_tx_token_put(dev);
	idr_init(&dev->mt76.token);

	mt792x_wpdma_reset(dev, true);

	mt76_for_each_q_rx(&dev->mt76, i) {
		napi_enable(&dev->mt76.napi[i]);
	}
	napi_enable(&dev->mt76.tx_napi);

	local_bh_disable();
	mt76_for_each_q_rx(&dev->mt76, i) {
		napi_schedule(&dev->mt76.napi[i]);
	}
	napi_schedule(&dev->mt76.tx_napi);
	local_bh_enable();

	dev->fw_assert = false;
	clear_bit(MT76_MCU_RESET, &dev->mphy.state);

	mt76_wr(dev, dev->irq_map->host_irq_enable,
		dev->irq_map->tx.all_complete_mask |
		dev->irq_map->rx.all_complete_mask |
		MT_INT_MCU_CMD);
	mt76_wr(dev, dev->pcie_reg->imask, 0xff);

	err = mt792xe_mcu_fw_pmctrl(dev);
	if (err)
		return err;

	err = __mt792xe_mcu_drv_pmctrl(dev);
	if (err)
		goto out;

	err = mt7925_run_firmware(dev);
	if (err)
		goto out;

	err = mt7925_mcu_set_eeprom(dev);
	if (err)
		goto out;

	err = mt7925_mac_init(dev);
	if (err)
		goto out;

	if (is_mt7927(&dev->mt76)) {
		err = mt7925_mcu_set_dbdc(&dev->mphy, true);
		if (err)
			goto out;
	}

	err = __mt7925_start(&dev->phy);
out:
	clear_bit(MT76_RESET, &dev->mphy.state);

	mt76_worker_enable(&dev->mt76.tx_worker);

	return err;
}

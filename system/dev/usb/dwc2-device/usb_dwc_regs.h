// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <zircon/hw/usb.h>

#define MAX_EPS_CHANNELS 16

#define DWC_EP_IN_SHIFT  0
#define DWC_EP_OUT_SHIFT 16

#define DWC_EP_IN_MASK   0x0000ffff
#define DWC_EP_OUT_MASK  0xffff0000

#define DWC_EP_IS_IN(ep)    ((ep) < 16)
#define DWC_EP_IS_OUT(ep)   ((ep) >= 16)

#define DWC_MAX_EPS    32

// converts a USB endpoint address to 0 - 31 index
// in endpoints -> 0 - 15
// out endpoints -> 17 - 31 (16 is unused)
#define DWC_ADDR_TO_INDEX(addr) (((addr) & 0xF) + (16 * !((addr) & USB_DIR_IN)))

#define DWC_REG_DATA_FIFO_START 0x1000
#define DWC_REG_DATA_FIFO(regs, ep)	((volatile uint32_t*)((uint8_t*)regs + (ep + 1) * 0x1000))

typedef union {
    uint32_t val;
    struct {
		uint32_t glblintrmsk                : 1;
		uint32_t hburstlen                  : 4;
		uint32_t dmaenable                  : 1;
		uint32_t reserved                   : 1;
		uint32_t nptxfemplvl_txfemplvl      : 1;
		uint32_t ptxfemplvl                 : 1;
		uint32_t reserved2                  : 12;
		uint32_t remmemsupp                 : 1;
		uint32_t notialldmawrit             : 1;
		uint32_t AHBSingle                  : 1;
		uint32_t reserved3                  : 8;
	};
} dwc_gahbcfg_t;

typedef union {
    uint32_t val;
    struct {
        uint32_t toutcal                    : 3;
        uint32_t phyif                      : 1;
        uint32_t ulpi_utmi_sel              : 1;
        uint32_t fsintf                     : 1;
        uint32_t physel                     : 1;
        uint32_t ddrsel                     : 1;
        uint32_t srpcap                     : 1;
        uint32_t hnpcap                     : 1;
        uint32_t usbtrdtim                  : 4;
        uint32_t reserved1                  : 1;
        uint32_t phylpwrclksel              : 1;
        uint32_t otgutmifssel               : 1;
        uint32_t ulpi_fsls                  : 1;
        uint32_t ulpi_auto_res              : 1;
        uint32_t ulpi_clk_sus_m             : 1;
        uint32_t ulpi_ext_vbus_drv          : 1;
        uint32_t ulpi_int_vbus_indicator    : 1;
        uint32_t term_sel_dl_pulse          : 1;
        uint32_t indicator_complement       : 1;
        uint32_t indicator_pass_through     : 1;
        uint32_t ulpi_int_prot_dis          : 1;
        uint32_t ic_usb_cap                 : 1;
        uint32_t ic_traffic_pull_remove     : 1;
        uint32_t tx_end_delay               : 1;
        uint32_t force_host_mode            : 1;
        uint32_t force_dev_mode             : 1;
        uint32_t reserved31                 : 1;
    };
} dwc_gusbcfg_t;
    
typedef union {
    uint32_t val;
    struct {
	    /* Core Soft Reset */
		uint32_t csftrst    : 1;
		/* Hclk Soft Reset */
		uint32_t hsftrst    : 1;
		/* Host Frame Counter Reset */
		uint32_t hstfrm     : 1;
		/* In Token Sequence Learning Queue Flush */
		uint32_t intknqflsh : 1;
		/* RxFIFO Flush */
		uint32_t rxfflsh    : 1;
		/* TxFIFO Flush */
		uint32_t txfflsh    : 1;
		/* TxFIFO Number */
		uint32_t txfnum     : 5;
		uint32_t reserved   : 19;
		/* DMA Request Signal */
		uint32_t dmareq     : 1;
		/* AHB Master Idle */
		uint32_t ahbidle    : 1;
    };
} dwc_grstctl_t;

typedef union {
        uint32_t val;
    struct {
        uint32_t curmode           : 1;
        uint32_t modemismatch      : 1;
        uint32_t otgintr           : 1;
        uint32_t sof_intr          : 1;
        uint32_t rxstsqlvl         : 1;
        uint32_t nptxfempty        : 1;
        uint32_t ginnakeff         : 1;
        uint32_t goutnakeff        : 1;
        uint32_t ulpickint         : 1;
        uint32_t i2cintr           : 1;
        uint32_t erlysuspend       : 1;
        uint32_t usbsuspend        : 1;
        uint32_t usbreset          : 1;
        uint32_t enumdone          : 1;
        uint32_t isooutdrop        : 1;
        uint32_t eopframe          : 1;
        uint32_t restoredone       : 1;
        uint32_t epmismatch        : 1;
        uint32_t inepintr          : 1;
        uint32_t outepintr         : 1;
        uint32_t incomplisoin      : 1;
        uint32_t incomplisoout     : 1;
        uint32_t fetsusp           : 1;
        uint32_t resetdet          : 1;
        uint32_t port_intr         : 1;
        uint32_t host_channel_intr : 1;
        uint32_t ptxfempty         : 1;
        uint32_t lpmtranrcvd       : 1;
        uint32_t conidstschng      : 1;
        uint32_t disconnect        : 1;
        uint32_t sessreqintr       : 1;
        uint32_t wkupintr          : 1;
    };
} dwc_interrupts_t;

typedef union {
    uint32_t val;
    struct {
    	uint32_t epnum      : 4;
    	uint32_t bcnt       : 11;
    	uint32_t dpid       : 2;
#define DWC_STS_DATA_UPDT		0x2	// OUT Data Packet
#define DWC_STS_XFER_COMP		0x3	// OUT Data Transfer Complete
#define DWC_DSTS_GOUT_NAK		0x1	// Global OUT NAK
#define DWC_DSTS_SETUP_COMP		0x4	// Setup Phase Complete
#define DWC_DSTS_SETUP_UPDT 0x6	// SETUP Packet
    	uint32_t pktsts     : 4;
    	uint32_t fn         : 4;
    	uint32_t reserved   : 7;
    };
} dwc_grxstsp_t;

typedef union {
	uint32_t val;
	struct {
		uint32_t mps        : 11;
#define DWC_DEP0CTL_MPS_64	 0
#define DWC_DEP0CTL_MPS_32	 1
#define DWC_DEP0CTL_MPS_16	 2
#define DWC_DEP0CTL_MPS_8	 3
		uint32_t nextep     : 4;
		uint32_t usbactep   : 1;
		uint32_t dpid       : 1;
		uint32_t naksts     : 1;
		uint32_t eptype     : 2;
		uint32_t snp        : 1;
		uint32_t stall      : 1;
		uint32_t txfnum     : 4;
		uint32_t cnak       : 1;
		uint32_t snak       : 1;
		uint32_t setd0pid   : 1;
		uint32_t setd1pid   : 1;
		uint32_t epdis      : 1;
		uint32_t epena      : 1;
	};
} dwc_depctl_t;

typedef union {
	uint32_t val;
	struct {
		/* Transfer size */
		uint32_t xfersize   : 19;
		/* Packet Count */
		uint32_t pktcnt     : 10;
		/* Multi Count */
		uint32_t mc         : 2;
		uint32_t reserved   : 1;
	};
} dwc_deptsiz_t;

typedef union {
	uint32_t val;
	struct {
		/* Transfer size */
		uint32_t xfersize   : 7;
		uint32_t reserved   : 12;
		/* Packet Count */
		uint32_t pktcnt     : 2;
		uint32_t reserved2  : 8;
		/* Setup Packet Count */
		uint32_t supcnt     : 2;
		uint32_t reserved3  : 1;
	};
} dwc_deptsiz0_t;

typedef union {
	uint32_t val;
	struct {
		/* Transfer complete mask */
		uint32_t xfercompl      : 1;
		/* Endpoint disable mask */
		uint32_t epdisabled     : 1;
		/* AHB Error mask */
		uint32_t ahberr         : 1;
		/* TimeOUT Handshake mask (non-ISOC EPs) */
		uint32_t timeout        : 1;
		/* IN Token received with TxF Empty mask */
		uint32_t intktxfemp     : 1;
		/* IN Token Received with EP mismatch mask */
		uint32_t intknepmis     : 1;
		/* IN Endpoint NAK Effective mask */
		uint32_t inepnakeff     : 1;
		uint32_t reserved       : 1;
		uint32_t txfifoundrn    : 1;
		/* BNA Interrupt mask */
		uint32_t bna            : 1;
		uint32_t reserved2      : 3;
		/* BNA Interrupt mask */
		uint32_t nak:1;
		uint32_t reserved3      : 18;
	};
} dwc_diepint_t;

typedef union {
	uint32_t val;
	struct {
		/* Transfer complete */
		uint32_t xfercompl      : 1;
		/* Endpoint disable  */
		uint32_t epdisabled     : 1;
		/* AHB Error */
		uint32_t ahberr         : 1;
		/* Setup Phase Done (contorl EPs) */
		uint32_t setup          : 1;
		/* OUT Token Received when Endpoint Disabled */
		uint32_t outtknepdis    : 1;
		uint32_t stsphsercvd    : 1;
		/* Back-to-Back SETUP Packets Received */
		uint32_t back2backsetup : 1;
		uint32_t reserved       : 1;
		/* OUT packet Error */
		uint32_t outpkterr      : 1;
		/* BNA Interrupt */
		uint32_t bna            : 1;

		uint32_t reserved2      : 1;
		/* Packet Drop Status */
		uint32_t pktdrpsts      : 1;
		/* Babble Interrupt */
		uint32_t babble         : 1;
		/* NAK Interrupt */
		uint32_t nak            : 1;
		/* NYET Interrupt */
		uint32_t nyet           : 1;
        /* Bit indicating setup packet received */
        uint32_t sr             : 1;
		uint32_t reserved3      : 16;
	};
} dwc_doepint_t;

typedef union {
    uint32_t val;
    struct {
        uint32_t devspd         : 2;
        uint32_t nzstsouthshk   : 1;
        uint32_t ena32khzs      : 1;
        uint32_t devaddr        : 7;
        uint32_t perfrint       : 2;
        uint32_t endevoutnak    : 1;
        uint32_t reserved       : 4;
        uint32_t epmscnt        : 5;
        uint32_t descdma        : 1;
        uint32_t perschintvl    : 2;
        uint32_t resvalid       : 6;
    };
} dwc_dcfg_t;

typedef union {
    uint32_t val;
    struct {
        /* Remote Wakeup */
        uint32_t rmtwkupsig     : 1;
        /* Soft Disconnect */
        uint32_t sftdiscon      : 1;
        /* Global Non-Periodic IN NAK Status */
        uint32_t gnpinnaksts    : 1;
        /* Global OUT NAK Status */
        uint32_t goutnaksts     : 1;
        /* Test Control */
        uint32_t tstctl         : 3;
        /* Set Global Non-Periodic IN NAK */
        uint32_t sgnpinnak      : 1;
        /* Clear Global Non-Periodic IN NAK */
        uint32_t cgnpinnak      : 1;
        /* Set Global OUT NAK */
        uint32_t sgoutnak       : 1;
        /* Clear Global OUT NAK */
        uint32_t cgoutnak       : 1;
        /* Power-On Programming Done */
        uint32_t pwronprgdone   : 1;
        /* Reserved */
        uint32_t reserved       : 1;
        /* Global Multi Count */
        uint32_t gmc            : 2;
        /* Ignore Frame Number for ISOC EPs */
        uint32_t ifrmnum        : 1;
        /* NAK on Babble */
        uint32_t nakonbble      : 1;
        /* Enable Continue on BNA */
        uint32_t encontonbna    : 1;
        /* Enable deep sleep besl reject feature*/
        uint32_t besl_reject    : 1;
        uint32_t reserved2      : 13;
    };
} dwc_dctl_t;

typedef union {
    uint32_t val;
    struct {
        /* Suspend Status */
        uint32_t suspsts    : 1;
        /* Enumerated Speed */
        uint32_t enumspd    : 2;
        /* Erratic Error */
        uint32_t errticerr  : 1;
        uint32_t reserved   : 4;
        /* Frame or Microframe Number of the received SOF */
        uint32_t soffn      : 14;
        uint32_t reserved2  : 10;
    };
} dwc_dsts_t;

typedef struct {
	/* Device IN Endpoint Control Register */
	dwc_depctl_t diepctl;

	uint32_t reserved;
	/* Device IN Endpoint Interrupt Register */
	dwc_diepint_t diepint;
	uint32_t reserved2;
	/* Device IN Endpoint Transfer Size */
	dwc_deptsiz_t dieptsiz;
	/* Device IN Endpoint DMA Address Register */
	uint32_t diepdma;
	/* Device IN Endpoint Transmit FIFO Status Register */
	uint32_t dtxfsts;
	/* Device IN Endpoint DMA Buffer Register */
	uint32_t diepdmab;
} dwc_depin_t;

typedef struct {
	/* Device OUT Endpoint Control Register */
	dwc_depctl_t doepctl;

	uint32_t reserved;
	/* Device OUT Endpoint Interrupt Register */
	dwc_doepint_t doepint;
	uint32_t reserved2;
	/* Device OUT Endpoint Transfer Size Register */
	dwc_deptsiz_t doeptsiz;
	/* Device OUT Endpoint DMA Address Register */
	uint32_t doepdma;
	uint32_t reserved3;
	/* Device OUT Endpoint DMA Buffer Register */
	uint32_t doepdmab;
} dwc_depout_t;

typedef union {
    uint32_t val;
    struct {
		/** Stop Pclk */
		uint32_t stoppclk:1;
		/** Gate Hclk */
		uint32_t gatehclk:1;
		/** Power Clamp */
		uint32_t pwrclmp:1;
		/** Reset Power Down Modules */
		uint32_t rstpdwnmodule:1;
		/** Reserved */
		uint32_t reserved:1;
		/** Enable Sleep Clock Gating (Enbl_L1Gating) */
		uint32_t enbl_sleep_gating:1;
		/** PHY In Sleep (PhySleep) */
		uint32_t phy_in_sleep:1;
		/** Deep Sleep*/
		uint32_t deep_sleep:1;
		uint32_t resetaftsusp:1;
		uint32_t restoremode:1;
		uint32_t reserved10_12:3;
		uint32_t ess_reg_restored:1;
		uint32_t prt_clk_sel:2;
		uint32_t port_power:1;
		uint32_t max_xcvrselect:2;
		uint32_t max_termsel:1;
		uint32_t mac_dev_addr:7;
		uint32_t p2hd_dev_enum_spd:2;
		uint32_t p2hd_prt_spd:2;
		uint32_t if_dev_mode:1;
	};
} dwc_pcgcctl_t;

typedef union {
    uint32_t val;
    struct {
		uint32_t nptxfspcavail:16;
		uint32_t nptxqspcavail:8;
		/** Top of the Non-Periodic Transmit Request Queue
		 *	- bit 24 - Terminate (Last entry for the selected
		 *	  channel/EP)
		 *	- bits 26:25 - Token Type
		 *	  - 2'b00 - IN/OUT
		 *	  - 2'b01 - Zero Length OUT
		 *	  - 2'b10 - PING/Complete Split
		 *	  - 2'b11 - Channel Halt
		 *	- bits 30:27 - Channel/EP Number
		 */
		uint32_t nptxqtop_terminate:1;
		uint32_t nptxqtop_token:2;
		uint32_t nptxqtop_chnep:4;
		uint32_t reserved:1;
	};
} dwc_gnptxsts_t;

typedef union  {
    uint32_t val;
    struct {
		uint32_t startaddr  : 16;
		uint32_t depth      : 16;
	};
} dwc_fifosiz_t;

typedef volatile struct {
    // OTG Control and Status Register
    uint32_t gotgctl;
    // OTG Interrupt Register
    uint32_t gotgint;
    // Core AHB Configuration Register
    dwc_gahbcfg_t gahbcfg;
    // Core USB Configuration Register
    dwc_gusbcfg_t gusbcfg;
    // Core Reset Register
    dwc_grstctl_t grstctl;
    // Core Interrupt Register
    dwc_interrupts_t gintsts;
    // Core Interrupt Mask Register
    dwc_interrupts_t gintmsk;
	// Receive Status Queue Read Register
    uint32_t grxstsr;
	// Receive Status Queue Read & POP Register
    dwc_grxstsp_t grxstsp;
	// Receive FIFO Size Register
    uint32_t grxfsiz;
	// Non Periodic Transmit FIFO Size Register
    dwc_fifosiz_t gnptxfsiz;
	// Non Periodic Transmit FIFO/Queue Status Register
    dwc_gnptxsts_t gnptxsts;
    // I2C Access Register
    uint32_t gi2cctl;
	// PHY Vendor Control Register
	uint32_t gpvndctl;
	// General Purpose Input/Output Register
	uint32_t ggpio;
	// User ID Register
	uint32_t guid;
	// Synopsys ID Register (Read Only)
	uint32_t gsnpsid;
	// User HW Config1 Register (Read Only)
	uint32_t ghwcfg1;
	// User HW Config2 Register (Read Only)
	uint32_t ghwcfg2;
	// User HW Config3 Register (Read Only)
	uint32_t ghwcfg3;
	// User HW Config4 Register (Read Only)
	uint32_t ghwcfg4;

    uint32_t reserved_030[(0x800 - 0x054) / sizeof(uint32_t)];

    // Device Configuration Register
    dwc_dcfg_t dcfg;
    // Device Control Register
    dwc_dctl_t dctl;
    // Device Status Register
    dwc_dsts_t dsts;
    uint32_t unused;
    // Device IN Endpoint Common Interrupt Mask Register
    dwc_diepint_t diepmsk;
    // Device OUT Endpoint Common Interrupt Mask Register
    dwc_doepint_t doepmsk;
    // Device All Endpoints Interrupt Register
    uint32_t daint;
    //Device All Endpoints Interrupt Mask Register
    uint32_t daintmsk;
    // Device IN Token Queue Read Register-1
    uint32_t dtknqr1;
    // Device IN Token Queue Read Register-2
    uint32_t dtknqr2;
    // Device VBUS discharge Register
    uint32_t dvbusdis;
    // Device VBUS pulse Register
    uint32_t dvbuspulse;
    // Device IN Token Queue Read Register-3 / Device Thresholding control register
    uint32_t dtknqr3_dthrctl;
    // Device IN Token Queue Read Register-4 / Device IN EPs empty Inr. Mask Register
    uint32_t dtknqr4_fifoemptymsk;
    // Device Each Endpoint Interrupt Register
    uint32_t deachint;
    //: Device Each Endpoint Interrupt Mask Register
    uint32_t deachintmsk;
    // Device Each In Endpoint Interrupt Mask Register
    uint32_t diepeachintmsk[MAX_EPS_CHANNELS];
    // Device Each Out Endpoint Interrupt Mask Register
    uint32_t doepeachintmsk[MAX_EPS_CHANNELS];

    uint32_t reserved_0x8C0[(0x900 - 0x8C0) / sizeof(uint32_t)];

    dwc_depin_t depin[MAX_EPS_CHANNELS];

    dwc_depout_t depout[MAX_EPS_CHANNELS];

    uint32_t reserved_0xD00[(0xE00 - 0xD00) / sizeof(uint32_t)];

    dwc_pcgcctl_t pcgcctl;

} dwc_regs_t;

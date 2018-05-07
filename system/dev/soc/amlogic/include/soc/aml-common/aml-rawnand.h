// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define AML_NAME "aml-nand"

#define P_NAND_CMD      (0x00)
#define P_NAND_CFG      (0x04)
#define P_NAND_DADR     (0x08)
#define P_NAND_IADR     (0x0c)
#define P_NAND_BUF      (0x10)
#define P_NAND_INFO     (0x14)
#define P_NAND_DC       (0x18)
#define P_NAND_ADR      (0x1c)
#define P_NAND_DL       (0x20)
#define P_NAND_DH       (0x24)
#define P_NAND_CADR     (0x28)
#define P_NAND_SADR     (0x2c)
#define P_NAND_PINS     (0x30)
#define P_NAND_VER      (0x38)

#define AML_CMD_DRD     (0x8<<14)
#define AML_CMD_IDLE    (0xc<<14)
#define AML_CMD_DWR     (0x4<<14)
#define AML_CMD_CLE     (0x5<<14)
#define AML_CMD_ALE     (0x6<<14)
#define AML_CMD_ADL     ((0<<16) | (3<<20))
#define AML_CMD_ADH     ((1<<16) | (3<<20))
#define AML_CMD_AIL     ((2<<16) | (3<<20))
#define AML_CMD_AIH     ((3<<16) | (3<<20))
#define AML_CMD_SEED    ((8<<16) | (3<<20))
#define AML_CMD_M2N     ((0<<17) | (2<<20))
#define AML_CMD_N2M     ((1<<17) | (2<<20))
#define AML_CMD_RB      (1<<20)
#define AML_CMD_IO6     ((0xb<<10)|(1<<18))

#define NAND_TWB_TIME_CYCLE     10

#define CMDRWGEN(cmd_dir, ran, bch, short, pagesize, pages) \
    ((cmd_dir) | (ran) << 19 | (bch) << 14 | \
     (short) << 13 | ((pagesize)&0x7f) << 6 | ((pages)&0x3f))

#define GENCMDDADDRL(adl, addr) \
    ((adl) | ((addr) & 0xffff))
#define GENCMDDADDRH(adh, addr) \
    ((adh) | (((addr) >> 16) & 0xffff))

#define GENCMDIADDRL(ail, addr) \
    ((ail) | ((addr) & 0xffff))
#define GENCMDIADDRH(aih, addr) \
    ((aih) | (((addr) >> 16) & 0xffff))

#define RB_STA(x) (1<<(26+x))

#define ECC_CHECK_RETURN_FF (-1)

#define DMA_BUSY_TIMEOUT 0x100000

#define MAX_CE_NUM      2

#define RAN_ENABLE      1

#define CLK_ALWAYS_ON   (0x01 << 28)
#define AML_CLK_CYCLE   6

/* nand flash controller delay 3 ns */
#define AML_DEFAULT_DELAY 3000

#define MAX_ECC_INDEX   10

typedef enum rawnand_addr_window {
    NANDREG_WINDOW = 0,
    CLOCKREG_WINDOW,
    ADDR_WINDOW_COUNT,  // always last
} rawnand_addr_window_t;

typedef struct {
    int ecc_strength;
    int user_mode;
    int rand_mode;
#define NAND_USE_BOUNCE_BUFFER          0x1
    int options;
    int bch_mode;
} aml_controller_t;

typedef struct {
    platform_device_protocol_t pdev;
    zx_device_t* zxdev;
    io_buffer_t mmio[ADDR_WINDOW_COUNT];
    thrd_t irq_thread;
    zx_handle_t irq_handle;
    bool enabled;
    aml_controller_t  controller_params;
    uint32_t chip_select;
    int controller_delay;
    uint32_t writesize;	/* NAND pagesize - bytes */
    uint32_t erasesize;	/* size of erase block - bytes */
    uint32_t erasesize_pages;
    uint32_t oobsize;	/* oob bytes per NAND page - bytes */
#define NAND_BUSWIDTH_16        0x00000002    
    uint32_t bus_width;	/* 16bit or 8bit ? */
    uint64_t            chipsize; /* MiB */
    uint32_t page_shift; /* NAND page shift */
    completion_t req_completion;
    struct {
        uint64_t ecc_corrected;
        uint64_t failed;        
    } stats;
    io_buffer_t data_buffer;
    io_buffer_t info_buffer;
    zx_handle_t bti_handle;
    void *info_buf, *data_buf;
    zx_paddr_t info_buf_paddr, data_buf_paddr;
} aml_rawnand_t;

static inline void set_bits(uint32_t *_reg, const uint32_t _value,
                            const uint32_t _start, const uint32_t _len)
{
    writel(((readl(_reg) & ~(((1L << (_len))-1) << (_start)))
            | ((uint32_t)((_value)&((1L<<(_len))-1)) << (_start))), _reg);
}

static inline void nandctrl_set_cfg(aml_rawnand_t *rawnand,
                                    uint32_t val)
{
    volatile uint8_t *reg = (volatile uint8_t*)
        io_buffer_virt(&rawnand->mmio[NANDREG_WINDOW]);

    writel(val, reg + P_NAND_CFG);
}

static inline void nandctrl_set_timing_async(aml_rawnand_t *rawnand,
                                             int bus_tim,
                                             int bus_cyc)
{
    volatile uint8_t *reg = (volatile uint8_t*)
        io_buffer_virt(&rawnand->mmio[NANDREG_WINDOW]);
    
    set_bits((uint32_t *)(reg + P_NAND_CFG),
             ((bus_cyc&31)|((bus_tim&31)<<5)|(0<<10)),
             0, 12);
}

static inline void nandctrl_send_cmd(aml_rawnand_t *rawnand,
                                     uint32_t cmd)
{
    volatile uint8_t *reg = (volatile uint8_t*)
        io_buffer_virt(&rawnand->mmio[NANDREG_WINDOW]);

    writel(cmd, reg + P_NAND_CMD);
}

enum {
    AML_ECC_NONE    = 0,
    /* bch8 with ecc page size of 512B */
    AML_ECC_BCH8,
    /* bch8 with ecc page size of 1024B */
    AML_ECC_BCH8_1K,
    AML_ECC_BCH24_1K,
    AML_ECC_BCH30_1K,
    AML_ECC_BCH40_1K,
    AML_ECC_BCH50_1K,
    AML_ECC_BCH60_1K,

    /*                                                                      
     * Short mode is special only for page 0 when inplement booting         
     * from nand. it means that using a small size(384B/8=48B) of ecc page  
     * with a fixed ecc mode. rom code use short mode to read page0 for     
     * getting nand parameter such as ecc, scramber and so on.              
     * For gxl serial, first page adopt short mode and 60bit ecc; for axg   
     * serial, adopt short mode and 8bit ecc.                               
     */
    AML_ECC_BCH_SHORT,
};

/*
 * Controller ECC, OOB, RAND parameters
 * XXX - This should be controller specific.
 * Hardcoding for now.
 */
struct aml_controller_params {
    int         ecc_strength;   /* # of ECC bits per ECC page */
    int         user_mode; /* OOB bytes every ECC page or per block ? */
    int         rand_mode; /* Randomize ? */
    int         bch_mode;
};

/*
 * In the case where user_mode == 2 (2 OOB bytes per ECC page),
 * the controller adds one of these structs *per* ECC page in
 * the info_buf. 
 */
struct __attribute__((packed)) aml_info_format {
    uint16_t info_bytes;
    uint8_t zero_cnt;    /* bit0~5 is valid */
    struct ecc_sta {
        uint8_t eccerr_cnt : 6;
        uint8_t notused : 1;
        uint8_t completed : 1;
    } ecc;
    uint32_t reserved;
};

static_assert(sizeof(struct aml_info_format) == 8,
              "sizeof(struct aml_info_format) must be exactly 8 bytes");

typedef struct nand_setup {
    union {
	uint32_t d32;
	struct {
	    unsigned cmd:22;
	    unsigned large_page:1;  // 22
	    unsigned no_rb:1;	    // 23 from efuse
	    unsigned a2:1;	    // 24
	    unsigned reserved25:1;  // 25
	    unsigned page_list:1;   // 26
	    unsigned sync_mode:2;   // 27 from efuse
	    unsigned size:2;        // 29 from efuse
	    unsigned active:1;	    // 31
	} b;
    } cfg;
    uint16_t id;
    uint16_t max;    // id:0x100 user, max:0 disable.
} nand_setup_t;

typedef struct _nand_cmd {
    u_int8_t type;
    u_int8_t val;
} nand_cmd_t;


typedef struct _ext_info {
    uint32_t read_info;   	//nand_read_info;
    uint32_t new_type;    	//new_nand_type;
    uint32_t page_per_blk;   	//pages_in_block;
    uint32_t xlc;		//slc=1, mlc=2, tlc=3.
    uint32_t ce_mask;
    /* copact mode: boot means whole uboot
     * it's easy to understood that copies of
     * bl2 and fip are the same.
     * discrete mode, boot means the fip only
     */
    uint32_t boot_num;
    uint32_t each_boot_pages;
    /* for comptible reason */
    uint32_t bbt_occupy_pages;
    uint32_t bbt_start_block;
} ext_info_t;

typedef struct _nand_page0 {
    nand_setup_t nand_setup;		//8
    unsigned char page_list[16];	//16
    nand_cmd_t retry_usr[32];		//64 (32 cmd max I/F)
    ext_info_t ext_info;		//64
} nand_page0_t;				//384 bytes max.



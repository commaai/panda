#define	RESP_NONE 0U
#define	RESP_R1 1U
#define	RESP_R1b 2U
#define	RESP_R2 3U
#define	RESP_R3 4U
#define	RESP_R6 5U
#define	RESP_R7 6U

#define CMD0                          0                 // GO_IDLE_STATE          (SDIO:RESP_NONE   SPI:RESP_R1)
#define CMD2                          2                 // ALL_SEND_CID           (SDIO:RESP_R2     SPI: - )
#define CMD3                          3                 // SEND_RELATIVE_ADDR     (SDIO:RESP_R6     SPI: - )
#define CMD7                          7                 // SELECT/DESELECT_CARD   (SDIO:RESP_R1b    SPI: - )
#define CMD8                          8                 // SEND_IF_COND           (SDIO:RESP_R7     SPI:RESP_R7)
#define CMD9                          9                 // SEND_CSD               (SDIO:RESP_R2     SPI:RESP_R1+D16)
#define CMD12                         12                // STOP_TRANSMISSION      (SDIO:RESP_R1b    SPI:RESP_R1b)
#define CMD13                         13                // SEND_STATUS            (SDIO:RESP_R1     SPI:RESP_R2)
#define CMD16                         16                // SET_BLOCKLEN           (SDIO:RESP_R1     SPI:RESP_R1)
#define CMD17                         17                // READ_SINGLE_BLOCK      (SDIO:RESP_R1     SPI:RESP_R1)
#define CMD18                         18                // READ_MULTIPLE_BLOCK    (SDIO:RESP_R1     SPI:RESP_R1)
#define CMD24                         24                // WRITE_BLOCK            (SDIO:RESP_R1     SPI:RESP_R1)
#define CMD25                         25                // WRITE_MULTIPLE_BLOCK   (SDIO:RESP_R1     SPI:RESP_R1)
#define CMD55                         55                // APP_CMD                (SDIO:RESP_R1     SPI:RESP_R1)
#define CMD58                         58                // READ_OCR               (SDIO: -          SPI:RESP_R3)
#define	ACMD6                         6                 // SET_BUS_WIDTH          (SDIO:RESP_R1     SPI: - )
#define	ACMD23                        23                // SET_WR_BLK_ERASE_COUNT (SDIO:RESP_R1     SPI:RESP_R1)
#define	ACMD41                        41                // SD_SEND_OP_COND        (SDIO:RESP_R3     SPI:RESP_R1)
#define	ACMD51                        51                // SEND_SCR               (SDIO:RESP_R1+D8  SPI:RESP_R1+D8)

#define CMD8_VHS1                     0x100             // 2.7-3.6V
#define CMD8_CHECK                    0xAA              // check pattern

#define	ACMD41_HCS                    0x40000000        // Host Capacity Support (0b: SDSC Only Host, 1b: SDHC or SDXC Supported)
#define	ACMD41_33_34V                 0x00100000        // 3.3-3.4V
#define	ACMD41_32_33V                 0x00080000        // 3.2-3.3V

#define	RESP_R3_CCS                   0x40000000        // card capacity status
#define	RESP_R3_BUSY                  0x80000000        // card power up status bit

#define	RESP_R1_CURRENT_STATE_MASK    0x1E00            // The state of the card field mask
#define	RESP_R1_CURRENT_STATE_IDLE    0x0000            // idle
#define	RESP_R1_CURRENT_STATE_READY   0x0200            // ready
#define	RESP_R1_CURRENT_STATE_IDENT   0x0400            // ident
#define	RESP_R1_CURRENT_STATE_STBY    0x0600            // stby
#define	RESP_R1_CURRENT_STATE_TRAN    0x0800            // tran
#define	RESP_R1_CURRENT_STATE_DATA    0x0A00            // data
#define	RESP_R1_CURRENT_STATE_RCV     0x0C00            // rcv
#define	RESP_R1_CURRENT_STATE_PRG     0x0E00            // prg
#define	RESP_R1_CURRENT_STATE_DIS     0x1000            // dis

#define SCR_SD_BUS_WIDTHS_1           0x10000           // 1 bit (DAT0)
#define SCR_SD_BUS_WIDTHS_4           0x40000           // 4 bit (DAT0-3)
#define SCR_SD_SECURITY_MASK          0x700000          // CPRM Security Version field mask
#define SCR_SD_SECURITY_SDSC          0x200000          // SDSC Card (Security Version 1.01)
#define SCR_SD_SECURITY_SDHC          0x300000          // SDHC Card (Security Version 2.00)
#define SCR_SD_SECURITY_SDXC          0x400000          // SDXC Card (Security Version 3.xx)


#define SDMMC_INIT_CLK_DIV            0x64U // div 200 (400kHz max)
#define SDMMC_TRANSFER_CLK_DIV        0x2U // div 4 (PLL1Q/4=20MHz) (25MHz max)

//--------------------------------------------
#define INIT_PROCESS_COUNT            1000      // Number of attempts to get a ready state
#define DATATIMEOUT                   0xFFFFFF  // A method for computing a realistic values from CSD is described in the specs

#define SDMMC_DCTRL_BLOCKSIZE_512			(9 << SDMMC_DCTRL_DBLOCKSIZE_Pos )
#define SDMMC_STA_ERRORS			        (SDMMC_STA_RXOVERR | SDMMC_STA_TXUNDERR | SDMMC_STA_DTIMEOUT | SDMMC_STA_DCRCFAIL )
#define SDMMC_ICR_STATIC              (SDMMC_ICR_CCRCFAILC | SDMMC_ICR_DCRCFAILC | SDMMC_ICR_CTIMEOUTC | \
                                       SDMMC_ICR_DTIMEOUTC | SDMMC_ICR_TXUNDERRC | SDMMC_ICR_RXOVERRC  | \
                                       SDMMC_ICR_CMDRENDC  | SDMMC_ICR_CMDSENTC  | SDMMC_ICR_DATAENDC  | \
                                       SDMMC_ICR_DBCKENDC )

#define SDMMC_BLOCK_SIZE              512U
#define SDMMC_BUFFER_SIZE             32U*SDMMC_BLOCK_SIZE

// for FIFO D1 and D2 works, but for IDMA only D1 works
__attribute__((section(".axisram"), aligned(32))) uint8_t idmabuf[SDMMC_BUFFER_SIZE];

typedef enum
{
	sd_err_ok = 0,
	sd_err_ctimeout,
	sd_err_ccrcfail,
	sd_err_dtimeout,
	sd_err_dcrcfail,
	sd_err_rxoverr,
	sd_err_txunderr,
	sd_err_stbiterr,
	sd_err_writefail,
	sd_err_unexpected_command,
	sd_err_not_supported,
	sd_err_wrong_status
} sd_error;

typedef enum
{
	CARD_SDSC,
	CARD_SDHC,
	CARD_SDXC
} sd_type;

typedef struct sd_info
{
	sd_type    type;            // type of the sd card
	uint32_t   rca;             // relative card address
	uint32_t   card_size;       // card size in sectors
	uint32_t   sect_size;       // sector size in bytes
	uint32_t   erase_size;      // erase block size in sectors
} sd_info_t;

static sd_info_t sdinfo;


sd_error send_command(uint32_t cmd, uint32_t arg, uint8_t resp, uint32_t *buf) {
	uint32_t cmd_waitresp = 0;

	switch(resp) {
	case RESP_NONE:
		// No response, expect CMDSENT flag
		cmd_waitresp = 0;
		break;
	case RESP_R1:
	case RESP_R1b:
	case RESP_R3:
	case RESP_R6:
	case RESP_R7:
		// Short response, expect CMDREND or CCRCFAIL flag
		cmd_waitresp = SDMMC_CMD_WAITRESP_0;
		break;
	case RESP_R2:
		// Long response, expect CMDREND or CCRCFAIL flag
		cmd_waitresp = SDMMC_CMD_WAITRESP_0 | SDMMC_CMD_WAITRESP_1;
		break;
	}

	SDMMC1->ARG = arg;
	SDMMC1->CMD = (uint32_t)(cmd & SDMMC_CMD_CMDINDEX) | (cmd_waitresp & SDMMC_CMD_WAITRESP) | SDMMC_CMD_CPSMEN;
	if (resp == RESP_NONE) {
		while (!(SDMMC1->STA & SDMMC_STA_CMDSENT));
		SDMMC1->ICR = SDMMC_ICR_CMDSENTC;
		return sd_err_ok;
	}
	while (!(SDMMC1->STA & (SDMMC_STA_CMDREND | SDMMC_STA_CCRCFAIL | SDMMC_STA_CTIMEOUT)));
	if (SDMMC1->STA & SDMMC_STA_CTIMEOUT) {
		SDMMC1->ICR = SDMMC_STA_CTIMEOUT;
		return sd_err_ctimeout;
	}
	// STM32F74xxx STM32F75xxx Errata sheet:
	// 2.12.1 Wrong CCRCFAIL status after a response without CRC is received (RESP_R3)
	if ((resp != RESP_R3) && (SDMMC1->STA & SDMMC_STA_CCRCFAIL)) {
		SDMMC1->ICR = SDMMC_STA_CCRCFAIL;
		return sd_err_ccrcfail;
	}
	SDMMC1->ICR = SDMMC_STA_CMDREND | SDMMC_STA_CCRCFAIL;
	switch(resp) {
	case RESP_R1:
	case RESP_R1b:
	case RESP_R3:
	case RESP_R6:
	case RESP_R7:
		buf[0] = SDMMC1->RESP1;
		break;
	case RESP_R2:
		buf[0] = SDMMC1->RESP1;
		buf[1] = SDMMC1->RESP2;
		buf[2] = SDMMC1->RESP3;
		buf[3] = SDMMC1->RESP4;
		break;
	default:
		break;
	}
	switch(resp) {
	case RESP_R1:
	case RESP_R1b:
	case RESP_R6:
	case RESP_R7:
		if (SDMMC1->RESPCMD != cmd) {
			return sd_err_unexpected_command;
		}
		break;
	default:
		break;
	}

	return sd_err_ok;
}

//--------------------------------------------
// Reads and parses the SD Configuration Register (SCR),
// calls only in Transfer State (tran)
sd_error read_scr(void) {
	sd_error err;
	uint32_t cnt;
	uint32_t *buf = (uint32_t*)idmabuf;
	uint32_t sd_sec;

	err = sd_err_ok;
	cnt = 0;
	// CMD16: Block length to 8 byte
	if ((err = send_command(CMD16, 8, RESP_R1, &buf[0])) != sd_err_ok) {
		return err;
	}
	// CMD55: Next command is an application specific
	if ((err = send_command(CMD55, sdinfo.rca << 16, RESP_R1, &buf[0])) != sd_err_ok) {
		return err;
	}
	// Receiving 8 data bytes
	SDMMC1->DTIMER = DATATIMEOUT;
	SDMMC1->DLEN = 8;
	SDMMC1->DCTRL = (SDMMC_DCTRL_DBLOCKSIZE_0 | SDMMC_DCTRL_DBLOCKSIZE_1) | \
	                 SDMMC_DCTRL_DTDIR | \
	                 SDMMC_DCTRL_DTEN;

	// ACMD51: Reads the SD Configuration Register (SCR)
	if ((err = send_command(ACMD51, 0, RESP_R1, &buf[0])) != sd_err_ok) {
		return err;
	}
	while (!(SDMMC1->STA & (SDMMC_STA_DATAEND | SDMMC_STA_RXOVERR | SDMMC_STA_DCRCFAIL | SDMMC_STA_DTIMEOUT))) {
		if (!(SDMMC1->STA & SDMMC_STA_RXFIFOE)) {
			buf[cnt++] = SDMMC1->FIFO;
		}
	}

	if (SDMMC1->STA & SDMMC_STA_DTIMEOUT) {
		err = sd_err_dtimeout;
	}
	if (SDMMC1->STA & SDMMC_STA_DCRCFAIL) {
		err = sd_err_dcrcfail;
	}
	if (SDMMC1->STA & SDMMC_STA_RXOVERR) {
		err = sd_err_rxoverr;
	}

	SDMMC1->ICR = SDMMC_ICR_STATIC;
	SDMMC1->DCTRL = 0;

	// swap bytes
	buf[0] = (buf[0] & 0x0000FFFF) << 16 | (buf[0] & 0xFFFF0000) >> 16;
	buf[0] = (buf[0] & 0x00FF00FF) << 8  | (buf[0] & 0xFF00FF00) >> 8;
	buf[1] = (buf[1] & 0x0000FFFF) << 16 | (buf[1] & 0xFFFF0000) >> 16;
	buf[1] = (buf[1] & 0x00FF00FF) << 8  | (buf[1] & 0xFF00FF00) >> 8;

	sd_sec = buf[0] & SCR_SD_SECURITY_MASK;
	if (sd_sec == SCR_SD_SECURITY_SDSC) {
		sdinfo.type = CARD_SDSC;
	}
	if (sd_sec == SCR_SD_SECURITY_SDHC) {
		sdinfo.type = CARD_SDHC;
	}
	if (sd_sec == SCR_SD_SECURITY_SDXC) {
		sdinfo.type = CARD_SDXC;
	}

	return err;
}

//--------------------------------------------
// Reads and parses the Card-Specific Data register (CSD)
sd_error read_csd(void) {
	sd_error err;
	uint32_t buf[4];
	uint32_t c_size;
	uint8_t c_size_mult;
	uint8_t read_bl_len;

	if ((err = send_command(CMD9, sdinfo.rca << 16, RESP_R2, &buf[0])) != sd_err_ok) {
		return err;
	}

	if (sdinfo.type == CARD_SDSC) {
		// CSD Version 1.0
		read_bl_len = (uint8_t)(buf[1] & 0x000F0000) >> 16;

		c_size  = (buf[1] & 0x00000300) << 2;
		c_size |= (buf[1] & 0x000000FF) << 2;
		c_size |= (buf[2] & 0xD0000000) >> 30;

		c_size_mult  = (buf[2] & 0x00030000) >> 15;
		c_size_mult |= (buf[2] & 0x00008000) >> 15;

		sdinfo.card_size = (c_size + 1) * (1 << (c_size_mult + 2)) * (1 << read_bl_len) / SDMMC_BLOCK_SIZE;
		sdinfo.sect_size = SDMMC_BLOCK_SIZE;
		// The erase operation can erase either one or multiple sectors
		sdinfo.erase_size = 1;
	} else {
		// CSD Version 2.0
		c_size  = (buf[1] & 0x0000003F) << 16;
		c_size |= (buf[2] & 0xFF000000) >> 16;
		c_size |= (buf[2] & 0x00FF0000) >> 16;

		sdinfo.card_size = (c_size + 1) * 1024; // in 512 byte sectors
		sdinfo.sect_size = SDMMC_BLOCK_SIZE;
		// The erase operation can erase either one or multiple sectors
		sdinfo.erase_size = 1;
	}
	return sd_err_ok;
}

//--------------------------------------------
sd_error sdmmc_reset(void) {
	sd_error err;
	uint32_t cnt;
	uint32_t buf[4];

  // Init SD card at 400kHz and one data line
	SDMMC1->CLKCR = SDMMC_INIT_CLK_DIV | SDMMC_CLKCR_PWRSAV | SDMMC_CLKCR_HWFC_EN;
	SDMMC1->POWER |= SDMMC_POWER_PWRCTRL;
  while ((SDMMC1->POWER & SDMMC_POWER_PWRCTRL) == 0) {};

	// command to send the card to SPI mode.
	send_command(CMD0, 0, RESP_NONE, &buf[0]);

	// The version 2.00 or later host shall issue
	// CMD8 and verify voltage before card initialization.
	err = send_command(CMD8, CMD8_VHS1 | CMD8_CHECK, RESP_R7, &buf[0]);
	if ((err != sd_err_unexpected_command) && (err != sd_err_ok)) {
		return err;
	}
	if (err == sd_err_unexpected_command) {
		// PLSS version 1.xx
		for (cnt = 0; cnt < INIT_PROCESS_COUNT; cnt++) {
			// CMD55: Next command is an application specific
			if ((err = send_command(CMD55, 0, RESP_R1, &buf[0])) != sd_err_ok) {
				return err;
			}
			// ACMD41 is a synchronization command used to negotiate the operation voltage range
			// and to poll the cards until they are out of their power-up sequence.
			if ((err = send_command(ACMD41, ACMD41_32_33V, RESP_R3, &buf[0])) != sd_err_ok) {
				return err;
			}
			if (buf[0] & RESP_R3_BUSY) {
				sdinfo.type = CARD_SDSC;
				break;
			}
		}
		if (cnt == INIT_PROCESS_COUNT) {
			return sd_err_not_supported;
		}
	} else {
		// PLSS version 2.00 or later
		if (buf[0] != (uint32_t)(CMD8_VHS1 | CMD8_CHECK)) {
			// voltage not supported
			return sd_err_not_supported;
		}

		// 2.7-3.6V supported
		for (cnt = 0; cnt < INIT_PROCESS_COUNT; cnt++) {
			// CMD55: Next command is an application specific
			if ((err = send_command(CMD55, 0, RESP_R1, &buf[0])) != sd_err_ok) {
				return err;
			}
			// ACMD41 is a synchronization command used to negotiate the operation voltage range
			// and to poll the cards until they are out of their power-up sequence.
			// ACMD41: Sends host capacity support information (HCS) and asks the
			// accessed card to send its operating condition register (OCR) content in the
			// response on the CMD line.
			if ((err = send_command(ACMD41, ACMD41_HCS | ACMD41_32_33V, RESP_R3, &buf[0])) != sd_err_ok) {
				return err;
			}
			if (buf[0] & RESP_R3_BUSY) {
				if (buf[0] & RESP_R3_CCS) {
					// SDHC or SDXC, it will be known later
					sdinfo.type = CARD_SDHC;
				} else {
					sdinfo.type = CARD_SDSC;
				}
				break;
			}
		}
		if (cnt == INIT_PROCESS_COUNT) {
			return sd_err_not_supported;
		}
	}
	// CMD2: Asks any card to send the CID numbers on the CMD line
	if ((err = send_command(CMD2, 0, RESP_R2, &buf[0])) != sd_err_ok) {
		return err;
	}
	// CMD3: Ask the card to publish a new relative address (RCA)
	// ==> Stand-by State (stby)
	if ((err = send_command(CMD3, 0, RESP_R6, &buf[0])) != sd_err_ok) {
		return err;
	}
	sdinfo.rca = buf[0] >> 16;
	// CMD9: Reads and parses the Card-Specific Data register (CSD)
	if ((err = read_csd()) != sd_err_ok) {
		return err;
	}
	// CMD7: Command toggles a card between the stand-by and transfer states
	// ==> Transfer State (tran)
	if ((err = send_command(CMD7, sdinfo.rca << 16, RESP_R1, &buf[0])) != sd_err_ok) {
		return err;
	}
	// CMD16->CMD55->ACMD51: Reads and parses the SD Configuration Register (SCR)
	if ((err = read_scr()) != sd_err_ok) {
		return err;
	}
	// CMD55: Next command is an application specific
	if ((err = send_command(CMD55, sdinfo.rca << 16, RESP_R1, &buf[0])) != sd_err_ok) {
		return err;
	}
	// ACMD6: Defines the 4 bits data bus width to be used for data transfer
	if ((err = send_command(ACMD6, 0x2, RESP_R1, &buf[0])) != sd_err_ok) {
		return err;
	}
  // Switch to fast clock and 4 data lines
	SDMMC1->CLKCR = SDMMC_TRANSFER_CLK_DIV | SDMMC_CLKCR_WIDBUS_0 | SDMMC_CLKCR_PWRSAV | SDMMC_CLKCR_NEGEDGE;

	return sd_err_ok;
}

//--------------------------------------------
sd_error sdmmc_read(uint8_t *rxbuf, uint32_t sector, uint32_t count) {
	sd_error err;
	uint32_t buf[4];
	uint32_t *dbuf;
	uint32_t cnt;
	uint32_t sta;

	dbuf = (uint32_t *)&rxbuf[0];

	// CMD13: Asks the selected card to send its status register
	if ((err = send_command(CMD13, sdinfo.rca << 16, RESP_R1, &buf[0])) != sd_err_ok) {
		return err;
	}
	// not Transfer State (tran)
	if ((buf[0] & RESP_R1_CURRENT_STATE_MASK) != RESP_R1_CURRENT_STATE_TRAN) {
		return sd_err_wrong_status;
	}
	if (sdinfo.type == CARD_SDSC) {
		// SDSC uses the 32-bit argument of memory access commands as byte address format
		// Block length is determined by CMD16, by default - 512 byte
		sector *= SDMMC_BLOCK_SIZE;
	}
	// Receiving 512 bytes data blocks
	SDMMC1->DTIMER = DATATIMEOUT;
	SDMMC1->DLEN = count * SDMMC_BLOCK_SIZE;
	SDMMC1->DCTRL = (SDMMC_DCTRL_DBLOCKSIZE_0 | SDMMC_DCTRL_DBLOCKSIZE_3) | // Data block size: (1001) 512 bytes
	                                     // (0) DMA disabled
	                 SDMMC_DCTRL_DTDIR | // Data transfer direction selection: (1) From card to controller
	                 SDMMC_DCTRL_DTEN;   // (1) Data transfer enabled bit

	if (count == 1) {
		// CMD17: Reads a block of the size selected by the SET_BLOCKLEN command (SDSC)
		// or exactly 512 byte (SDHC and SDXC)
		// ==> Sending-data State (data)
		if ((err = send_command(CMD17, sector, RESP_R1, &buf[0])) != sd_err_ok) {
			return err;
		}
	} else {
		// CMD18: Continuously transfers data blocks from card to host
		// until interrupted by a STOP_TRANSMISSION command
		// ==> Sending-data State (data)
		if ((err = send_command(CMD18, sector, RESP_R1, &buf[0])) != sd_err_ok) {
			return err;
		}
	}

	while (!(SDMMC1->STA & (SDMMC_STA_RXOVERR | SDMMC_STA_DCRCFAIL | SDMMC_STA_DTIMEOUT | SDMMC_STA_DATAEND))) {
		while (SDMMC1->STA & SDMMC_STA_RXFIFOHF) {
			for (cnt = 0; cnt < 8; cnt++) {
				*(dbuf + cnt) = SDMMC1->FIFO;
			}
			dbuf += 8;
		}
	}

	SDMMC1->DCTRL = 0;

	if (count > 1) {
		// CMD12: Forces the card to stop transmission
		// if count == 1, "operation complete" is automatic
		// ==> Transfer State (tran)
		if ((err = send_command(CMD12, 0, RESP_R1b, &buf[0])) != sd_err_ok) {
			SDMMC1->ICR = SDMMC_ICR_STATIC;
			return err;
		}
	}

	sta = SDMMC1->STA;
	SDMMC1->ICR = SDMMC_ICR_STATIC;

	if (sta & SDMMC_STA_DTIMEOUT) {
		return sd_err_dtimeout;
	}
	if (sta & SDMMC_STA_DCRCFAIL) {
		return sd_err_dcrcfail;
	}
	if (sta & SDMMC_STA_RXOVERR) {
		return sd_err_rxoverr;
	}
	return sd_err_ok;
}

// FIXME: IDMA does not work for read, timeout
sd_error sdmmc_read_idma(uint8_t *rxbuf, uint32_t sector, uint32_t count) {
	sd_error err;
	uint32_t buf[4];
	uint32_t *dbuf;
	uint32_t sta;
  uint8_t dir = 1;

	dbuf = (uint32_t *)&rxbuf[0];

  SDMMC1->DCTRL = 0; // reset DPSM

	// CMD13: Asks the selected card to send its status register
	if ((err = send_command(CMD13, sdinfo.rca << 16, RESP_R1, &buf[0])) != sd_err_ok) {
		return err;
	}
	// not Transfer State (tran)
	if ((buf[0] & RESP_R1_CURRENT_STATE_MASK) != RESP_R1_CURRENT_STATE_TRAN) {
		return sd_err_wrong_status;
	}

	if (sdinfo.type == CARD_SDSC) {
		// SDSC uses the 32-bit argument of memory access commands as byte address format
		// Block length is determined by CMD16, by default - 512 byte
		sector *= SDMMC_BLOCK_SIZE;
	}
	// Sending 512 bytes data blocks
  SDMMC1->DTIMER = DATATIMEOUT;
  SDMMC1->DCTRL |= SDMMC_DCTRL_FIFORST;
  SDMMC1->DLEN = count * SDMMC_BLOCK_SIZE;
  SDMMC1->MASK=0; // TODO: mask only FIFO interrupts??
  SDMMC1->IDMABASE0 = (uint32_t)dbuf;
  SDMMC1->DCTRL = SDMMC_DCTRL_BLOCKSIZE_512 | (dir & SDMMC_DCTRL_DTDIR);  //Direction. 0=Controller to card, 1=Card to Controller


	if (count == 1) {
		// CMD17: Reads a block of the size selected by the SET_BLOCKLEN command (SDSC)
		// or exactly 512 byte (SDHC and SDXC)
		// ==> Sending-data State (data)
		if ((err = send_command(CMD17, sector, RESP_R1, &buf[0])) != sd_err_ok) {
			return err;
		}
	} else {
		// CMD18: Continuously transfers data blocks from card to host
		// until interrupted by a STOP_TRANSMISSION command
		// ==> Sending-data State (data)
		if ((err = send_command(CMD18, sector, RESP_R1, &buf[0])) != sd_err_ok) {
			return err;
		}
	}

  SDMMC1->ICR = SDMMC_ICR_STATIC;
  SDMMC1->IDMACTRL = SDMMC_IDMA_IDMAEN;
	SDMMC1->DCTRL |= SDMMC_DCTRL_DTEN; //DPSM is enabled

  // TODO: this should be changed for IRQ instead of blocking
  while((SDMMC1->STA & (SDMMC_STA_DATAEND|SDMMC_STA_ERRORS)) == 0){__NOP();};

	SDMMC1->DCTRL = 0;

	if (count > 1) {
		// CMD12: Forces the card to stop transmission
		// if count == 1, "operation complete" is automatic
		// ==> Transfer State (tran)
		if ((err = send_command(CMD12, 0, RESP_R1b, &buf[0])) != sd_err_ok) {
			SDMMC1->ICR = SDMMC_ICR_STATIC;
			return err;
		}
	}

	sta = SDMMC1->STA;
	SDMMC1->ICR = SDMMC_ICR_STATIC;

	if (sta & SDMMC_STA_DTIMEOUT) {
		return sd_err_dtimeout;
	}
	if (sta & SDMMC_STA_DCRCFAIL) {
		return sd_err_dcrcfail;
	}
	if (sta & SDMMC_STA_TXUNDERR) {
		return sd_err_txunderr;
	}
	return sd_err_ok;
}

//--------------------------------------------
sd_error sdmmc_write(uint8_t *txbuf, uint32_t sector, uint32_t count) {
	sd_error err;
	uint32_t buf[4];
	uint32_t *dbuf;
	uint32_t cnt;
	uint32_t sta;

	dbuf = (uint32_t *)&txbuf[0];

	// CMD13: Asks the selected card to send its status register
	if ((err = send_command(CMD13, sdinfo.rca << 16, RESP_R1, &buf[0])) != sd_err_ok) {
		return err;
	}
	// not Transfer State (tran)
	if ((buf[0] & RESP_R1_CURRENT_STATE_MASK) != RESP_R1_CURRENT_STATE_TRAN) {
		return sd_err_wrong_status;
	}

	if (sdinfo.type == CARD_SDSC) {
		// SDSC uses the 32-bit argument of memory access commands as byte address format
		// Block length is determined by CMD16, by default - 512 byte
		sector *= SDMMC_BLOCK_SIZE;
	}
	// Sending 512 bytes data blocks
	SDMMC1->DTIMER = DATATIMEOUT;
	SDMMC1->DLEN = count * SDMMC_BLOCK_SIZE;
	SDMMC1->DCTRL = SDMMC_DCTRL_BLOCKSIZE_512 | // Data block size: (1001) 512 bytes
	                                     // (0) DMA disabled
	                                     // Data transfer direction selection: (0) From controller to card
	                 SDMMC_DCTRL_DTEN;   // (1) Data transfer enabled bit

	if (count == 1) {
		// CMD24: Write a block of the size selected by the SET_BLOCKLEN command (SDSC)
		// or exactly 512 byte (SDHC and SDXC)
		// ==> Receive-data State (rcv)
		if ((err = send_command(CMD24, sector, RESP_R1, &buf[0])) != sd_err_ok) {
			return err;
		}
	} else {
		// CMD55: Next command is an application specific
		if ((err = send_command(CMD55, sdinfo.rca << 16, RESP_R1, &buf[0])) != sd_err_ok) {
			return err;
		}
		// ACMD23: Set the number of write blocks to be pre-erased before writing 
		// (to be used for faster Multiple Block WR command)
		if ((err = send_command(ACMD23, count, RESP_R1, &buf[0])) != sd_err_ok) {
			return err;
		}
		// CMD25: Continuously writes blocks of data until a STOP_TRANSMISSION follows.
		// ==> Receive-data State (rcv)
		if ((err = send_command(CMD25, sector, RESP_R1, &buf[0])) != sd_err_ok) {
			return err;
		}
	}
	while (!(SDMMC1->STA & (SDMMC_STA_TXUNDERR | SDMMC_STA_DCRCFAIL | SDMMC_STA_DTIMEOUT | SDMMC_STA_DATAEND | SDMMC_STA_DBCKEND))) {
		while (SDMMC1->STA & SDMMC_STA_TXFIFOHE) {
			for (cnt = 0; cnt < 8; cnt++) {
				SDMMC1->FIFO = *(dbuf + cnt);
			}
			dbuf += 8;
		}
	}
	SDMMC1->DCTRL = 0;

	if (count > 1) {
		// CMD12: Forces the card to stop receiving
		// if count == 1, "transfer end" is automatic
		// ==> Programming State (prg) ==> Transfer State (tran)
		if ((err = send_command(CMD12, 0, RESP_R1b, &buf[0])) != sd_err_ok) {
			SDMMC1->ICR = SDMMC_ICR_STATIC;
			return err;
		}
	}

	sta = SDMMC1->STA;
	SDMMC1->ICR = SDMMC_ICR_STATIC;

	if (sta & SDMMC_STA_DTIMEOUT) {
		return sd_err_dtimeout;
	}
	if (sta & SDMMC_STA_DCRCFAIL) {
		return sd_err_dcrcfail;
	}
	if (sta & SDMMC_STA_TXUNDERR) {
		return sd_err_txunderr;
	}

	return sd_err_ok;
}


sd_error sdmmc_write_idma(uint8_t *txbuf, uint32_t sector, uint32_t count) {
	sd_error err;
	uint32_t buf[4];
	uint32_t *dbuf;
	uint32_t sta;
  uint8_t dir = 0;

	dbuf = (uint32_t *)&txbuf[0];

  SDMMC1->DCTRL = 0; // reset DPSM

	// CMD13: Asks the selected card to send its status register
	if ((err = send_command(CMD13, sdinfo.rca << 16, RESP_R1, &buf[0])) != sd_err_ok) {
		return err;
	}
	// not Transfer State (tran)
	if ((buf[0] & RESP_R1_CURRENT_STATE_MASK) != RESP_R1_CURRENT_STATE_TRAN) {
		return sd_err_wrong_status;
	}

	if (sdinfo.type == CARD_SDSC) {
		// SDSC uses the 32-bit argument of memory access commands as byte address format
		// Block length is determined by CMD16, by default - 512 byte
		sector *= SDMMC_BLOCK_SIZE;
	}
	// Sending 512 bytes data blocks
  SDMMC1->DTIMER = DATATIMEOUT;
  SDMMC1->DCTRL |= SDMMC_DCTRL_FIFORST;
  SDMMC1->DLEN = count * SDMMC_BLOCK_SIZE;
  SDMMC1->MASK=0; // TODO: mask only FIFO interrupts??
  SDMMC1->IDMABASE0 = (uint32_t)dbuf;
  SDMMC1->DCTRL = SDMMC_DCTRL_BLOCKSIZE_512 | (dir & SDMMC_DCTRL_DTDIR);  //Direction. 0=Controller to card, 1=Card to Controller


	if (count == 1) {
		// CMD24: Write a block of the size selected by the SET_BLOCKLEN command (SDSC)
		// or exactly 512 byte (SDHC and SDXC)
		// ==> Receive-data State (rcv)
		if ((err = send_command(CMD24, sector, RESP_R1, &buf[0])) != sd_err_ok) {
			return err;
		}
	} else {
		// CMD55: Next command is an application specific
		if ((err = send_command(CMD55, sdinfo.rca << 16, RESP_R1, &buf[0])) != sd_err_ok) {
			return err;
		}
		// ACMD23: Set the number of write blocks to be pre-erased before writing 
		// (to be used for faster Multiple Block WR command)
		if ((err = send_command(ACMD23, count, RESP_R1, &buf[0])) != sd_err_ok) {
			return err;
		}
		// CMD25: Continuously writes blocks of data until a STOP_TRANSMISSION follows.
		// ==> Receive-data State (rcv)
		if ((err = send_command(CMD25, sector, RESP_R1, &buf[0])) != sd_err_ok) {
			return err;
		}
	}

  SDMMC1->ICR = SDMMC_ICR_STATIC;
  SDMMC1->IDMACTRL = SDMMC_IDMA_IDMAEN;
	SDMMC1->DCTRL |= SDMMC_DCTRL_DTEN; //DPSM is enabled

  // TODO: this should be changed for IRQ instead of blocking
  while((SDMMC1->STA & (SDMMC_STA_DATAEND|SDMMC_STA_ERRORS)) == 0){__NOP();};

	SDMMC1->DCTRL = 0;

	if (count > 1) {
		// CMD12: Forces the card to stop receiving
		// if count == 1, "transfer end" is automatic
		// ==> Programming State (prg) ==> Transfer State (tran)
		if ((err = send_command(CMD12, 0, RESP_R1b, &buf[0])) != sd_err_ok) {
			SDMMC1->ICR = SDMMC_ICR_STATIC;
			return err;
		}
	}

	sta = SDMMC1->STA;
	SDMMC1->ICR = SDMMC_ICR_STATIC;

	if (sta & SDMMC_STA_DTIMEOUT) {
		return sd_err_dtimeout;
	}
	if (sta & SDMMC_STA_DCRCFAIL) {
		return sd_err_dcrcfail;
	}
	if (sta & SDMMC_STA_TXUNDERR) {
		return sd_err_txunderr;
	}
	return sd_err_ok;
}

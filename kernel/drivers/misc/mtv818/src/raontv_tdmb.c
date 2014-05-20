/******************************************************************************** 
* (c) COPYRIGHT 2010 RAONTECH, Inc. ALL RIGHTS RESERVED.
* 
* This software is the property of RAONTECH and is furnished under license by RAONTECH.                
* This software may be used only in accordance with the terms of said license.                         
* This copyright noitce may not be remoced, modified or obliterated without the prior                  
* written permission of RAONTECH, Inc.                                                                 
*                                                                                                      
* This software may not be copied, transmitted, provided to or otherwise made available                
* to any other person, company, corporation or other entity except as specified in the                 
* terms of said license.                                                                               
*                                                                                                      
* No right, title, ownership or other interest in the software is hereby granted or transferred.       
*                                                                                                      
* The information contained herein is subject to change without notice and should 
* not be construed as a commitment by RAONTECH, Inc.                                                                    
* 
* TITLE 	  : RAONTECH TV T-DMB services source file. 
*
* FILENAME    : raontv_tdmb.c
*
* DESCRIPTION : 
*		Library of routines to initialize, and operate on, the RAONTECH T-DMB demod.
*
********************************************************************************/

/******************************************************************************** 
* REVISION HISTORY
*
*    DATE	  	  NAME				REMARKS
* ----------  -------------    --------------------------------------------------
* 09/27/2010  Ko, Kevin        Creat for CS Realease
*             /Yang, Maverick  1.Reformating for CS API
*                              2.pll table, ADC clock switching, SCAN function, 
*								 FM function added..
* 04/09/2010  Yang, Maverick   REV1 SETTING 
* 01/25/2010  Yang, Maverick   Created.                                                              
********************************************************************************/

#include "raontv_rf.h"


#ifdef RTV_TDMB_ENABLE

#undef OFDM_PAGE
#define OFDM_PAGE	0x6

#undef FEC_PAGE
#define FEC_PAGE	0x09


#define MAX_NUM_TDMB_SUB_CH		64

/* Registered sub channel Table. */
typedef struct
{
	UINT 						nSubChID;
	UINT 						nHwSubChIdx;
	E_RTV_TDMB_SERVICE_TYPE 	eServiceType;	
	UINT						nThresholdSize;
} RTV_TDMB_REG_SUBCH_INFO;


#if (RTV_MAX_NUM_SUB_CHANNEL_USED == 1) /* Single Sub Channel */
	#define TDMB_MSC0_SUBCH_USE_MASK		0x00 /* NA */
	#define TDMB_MSC1_SUBCH_USE_MASK		0x01 /* SUBCH 0 */

#else /* Multi Sub Channel */
	#define TDMB_MSC0_SUBCH_USE_MASK		0x78 /* SUBCH 3,4,5,6 */
	#define TDMB_MSC1_SUBCH_USE_MASK		0x01 /* SUBCH 0 */
	
	#define TDMB_MSC_SUBCH_USE_MASKS (TDMB_MSC0_SUBCH_USE_MASK|TDMB_MSC1_SUBCH_USE_MASK)
#endif

#ifdef RTV_CIF_MODE_ENABLED
	extern void rtvCIFDEC_Init(void); 
    extern void rtvCIFDEC_Deinit(void);
	extern void rtvCIFDEC_DeleteSubChannelID(UINT nSubChID);
	extern void rtvCIFDEC_AddSubChannelID(UINT nSubChID);		
#endif


static RTV_TDMB_REG_SUBCH_INFO g_atTdmbRegSubchInfo[NUM_ATTECHED_RTV_CHIP][RTV_MAX_NUM_SUB_CHANNEL_USED];
static UINT g_aRegSubChArrayIdxBits[NUM_ATTECHED_RTV_CHIP];
static UINT g_aRtvUsedHwSubChIdxBits[NUM_ATTECHED_RTV_CHIP];
static U32 g_aTdmbPrevChFreqKHz[NUM_ATTECHED_RTV_CHIP]; /* Master and Slave*/
static BOOL g_afRtvTdmbConfiguredFIC[NUM_ATTECHED_RTV_CHIP];

#define SUBCH_GRP_IDX(sub_ch_id)	(sub_ch_id >> 5) // Divide by 32
#define MOD32(x)                    			(x & 31)
static U32 g_aRegSubChIdBits[NUM_ATTECHED_RTV_CHIP][2]; /* Used sub channel ID bits. [0]: 0 ~ 31, [1]: 32 ~ 63 */


/*==============================================================================
 * Replace the below code to eliminates the sqrt() and log10() functions.
 * In addtion, to eliminates floating operation, we multiplied by RTV_TDMB_SNR_DIVIDER to the floating SNR.
 * SNR = (double)(100/(sqrt((double)data) - log10((double)data)*log10((double)data)) -7);
 *============================================================================*/
static const U16 g_awSNR_15_160[] = 
{
	33163/* 15 */, 32214/* 16 */, 31327/* 17 */, 30496/* 18 */, 29714/* 19 */, 
	28978/* 20 */, 28281/* 21 */, 27622/* 22 */, 26995/* 23 */, 26400/* 24 */, 
	25832/* 25 */, 25290/* 26 */, 24772/* 27 */, 24277/* 28 */, 23801/* 29 */, 
	23345/* 30 */, 22907/* 31 */, 22486/* 32 */, 22080/* 33 */, 21690/* 34 */, 
	21313/* 35 */, 20949/* 36 */, 20597/* 37 */, 20257/* 38 */, 19928/* 39 */, 
	19610/* 40 */, 19301/* 41 */, 19002/* 42 */, 18712/* 43 */, 18430/* 44 */, 
	18156/* 45 */, 17890/* 46 */, 17632/* 47 */, 17380/* 48 */, 17135/* 49 */, 
	16897/* 50 */, 16665/* 51 */, 16438/* 52 */, 16218/* 53 */, 16002/* 54 */, 
	15792/* 55 */, 15587/* 56 */, 15387/* 57 */, 15192/* 58 */, 15001/* 59 */, 
	14814/* 60 */, 14631/* 61 */, 14453/* 62 */, 14278/* 63 */, 14107/* 64 */, 
	13939/* 65 */, 13775/* 66 */, 13615/* 67 */, 13457/* 68 */, 13303/* 69 */, 
	13152/* 70 */, 13004/* 71 */, 12858/* 72 */, 12715/* 73 */, 12575/* 74 */, 
	12438/* 75 */, 12303/* 76 */, 12171/* 77 */, 12041/* 78 */, 11913/* 79 */, 
	11788/* 80 */, 11664/* 81 */, 11543/* 82 */, 11424/* 83 */, 11307/* 84 */, 
	11192/* 85 */, 11078/* 86 */, 10967/* 87 */, 10857/* 88 */, 10749/* 89 */, 
	10643/* 90 */, 10539/* 91 */, 10436/* 92 */, 10334/* 93 */, 10235/* 94 */, 
	10136/* 95 */, 10039/* 96 */, 9944/* 97 */, 9850/* 98 */, 9757/* 99 */, 
	9666/* 100 */, 9576/* 101 */, 9487/* 102 */, 9400/* 103 */, 9314/* 104 */, 
	9229/* 105 */, 9145/* 106 */, 9062/* 107 */, 8980/* 108 */, 8900/* 109 */, 
	8820/* 110 */, 8742/* 111 */, 8664/* 112 */, 8588/* 113 */, 8512/* 114 */, 
	8438/* 115 */, 8364/* 116 */, 8292/* 117 */, 8220/* 118 */, 8149/* 119 */, 
	8079/* 120 */, 8010/* 121 */, 7942/* 122 */, 7874/* 123 */, 7807/* 124 */, 
	7742/* 125 */, 7676/* 126 */, 7612/* 127 */, 7548/* 128 */, 7485/* 129 */, 
	7423/* 130 */, 7362/* 131 */, 7301/* 132 */, 7241/* 133 */, 7181/* 134 */, 
	7123/* 135 */, 7064/* 136 */, 7007/* 137 */, 6950/* 138 */, 6894/* 139 */, 
	6838/* 140 */, 6783/* 141 */, 6728/* 142 */, 6674/* 143 */, 6621/* 144 */, 
	6568/* 145 */, 6516/* 146 */, 6464/* 147 */, 6412/* 148 */, 6362/* 149 */, 
	6311/* 150 */, 6262/* 151 */, 6212/* 152 */, 6164/* 153 */, 6115/* 154 */, 
	6067/* 155 */, 6020/* 156 */, 5973/* 157 */, 5927/* 158 */, 5881/* 159 */, 
	5835/* 160 */
};



static void tdmb_InitTOP(void)
{
	RTV_REG_MAP_SEL(OFDM_PAGE);
    RTV_REG_SET(0x07, 0x08); 
	RTV_REG_SET(0x05, 0x17); 
	RTV_REG_SET(0x06, 0x10);	
	RTV_REG_SET(0x0A, 0x00);   
}

//============================================================================
// Name    : tdmb_InitCOMM
// Action  : MAP SEL COMM Register Init
// Input   : Chip Address
// Output  : None
//============================================================================
static void tdmb_InitCOMM(void)
{
	RTV_REG_MAP_SEL(COMM_PAGE);
	RTV_REG_SET(0x10, 0x91);	  
	RTV_REG_SET(0xE1, 0x00);
	RTV_REG_SET(0x35, 0X8B);
	RTV_REG_SET(0x3B, 0x3C);   

	RTV_REG_SET(0x36, 0x67);   
	RTV_REG_SET(0x3A, 0x0F);	   

	RTV_REG_SET(0x3C,0x20); 
	RTV_REG_SET(0x3D,0x0B);   
	RTV_REG_SET(0x3D,0x09); 

#ifndef RTV_CIF_MODE_ENABLED /* Individual Mode */	 
	RTV_REG_SET(0xA6, 0x10); 
	
#else /* CIF mode. */
  #if defined(RTV_IF_MPEG2_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE) || defined(RTV_IF_QUALCOMM_TSIF)
  	RTV_REG_SET(0xA6, 0x30); 
  #else
  	RTV_REG_SET(0xA6, 0x10);  
  #endif	
#endif	

	RTV_REG_SET(0xAA, 0x01); // Enable 0x47 insertion to video frame.
}

//============================================================================
// Name    : tdmb_InitHOST
// Action  : MAP SEL HOST Register Init
// Input   : Chip Address
// Output  : None
//============================================================================
static void tdmb_InitHOST(void)
{
	RTV_REG_MAP_SEL(HOST_PAGE);
	RTV_REG_SET(0x10, 0x00);
	RTV_REG_SET(0x13,0x16);
	RTV_REG_SET(0x14,0x00);
	RTV_REG_SET(0x19,0x0A);
	RTV_REG_SET(0xF0,0x00);
	RTV_REG_SET(0xF1,0x00);
	RTV_REG_SET(0xF2,0x00);
	RTV_REG_SET(0xF3,0x00);
	RTV_REG_SET(0xF4,0x00);
	RTV_REG_SET(0xF5,0x00);
	RTV_REG_SET(0xF6,0x00);
	RTV_REG_SET(0xF7,0x00);
	RTV_REG_SET(0xF8,0x00);	
    RTV_REG_SET(0xFB,0xFF);  

}


//============================================================================
// Name    : tdmb_InitOFDM
// Action  : MAP SEL OFDM Register Init
// Input   : Chip Address
// Output  : None
//============================================================================
static void tdmb_InitOFDM(void)
{
	U8 INV_MODE;      
	U8 FREQ_PNCO_HL;  
	U8 FREQ_PNCO_HH;  
	U8 PWM_COM;       
	U8 WAGC_COM;      
	U8 AGC_MODE;      
	U8 POST_INIT;     
	U8 AGC_CYCLE;     

	INV_MODE = 1;		
	PWM_COM = 0x08;
	FREQ_PNCO_HL = 0x00;   
	FREQ_PNCO_HH = 0x00;
	WAGC_COM = 0x03;
	AGC_MODE = 0x06; 
	POST_INIT = 0x09;
	AGC_CYCLE = 0x10;

	RTV_REG_MAP_SEL(OFDM_PAGE);

	if(g_eRtvCountryBandType == RTV_COUNTRY_BAND_KOREA)
	{
		RTV_REG_SET(0x11,0x8e); 
	}
	
	RTV_REG_SET(0x12,0x04); 
	
	RTV_REG_SET(0x13,0x72); 
	RTV_REG_SET(0x14,0x63); 
	RTV_REG_SET(0x15,0x64); 

	RTV_REG_SET(0x16,0x6C); 

	RTV_REG_SET(0x38,0x01);	

    RTV_REG_SET(0x20,0x5B); 

    RTV_REG_SET(0x25,0x09);

    RTV_REG_SET(0x44,0x00 | (POST_INIT)); 

	RTV_REG_SET(0x46,0xA0); 
	RTV_REG_SET(0x47,0x0F);

	RTV_REG_SET(0x48,0xB8); 
	RTV_REG_SET(0x49,0x0B);  
	RTV_REG_SET(0x54,0x58); 

	RTV_REG_SET(0x55,0x06); 
	
	RTV_REG_SET(0x56,0x00 | (AGC_CYCLE));         

	RTV_REG_SET(0x59,0x51); 
                                            
	RTV_REG_SET(0x5A,0x1C); 

	RTV_REG_SET(0x6D,0x00); 
	RTV_REG_SET(0x8B,0x24); 

	RTV_REG_SET(0x6B,0x2D); 
	RTV_REG_SET(0x85,0x32); 
	RTV_REG_SET(0x8E,0x01); 

	RTV_REG_SET(0x33, 0x00 | (INV_MODE<<1)); 
	RTV_REG_SET(0x53,0x00 | (AGC_MODE)); 

	RTV_REG_SET(0x6F,0x00 | (WAGC_COM)); 
	
	RTV_REG_SET(0xBA,PWM_COM);

	switch( g_aeRtvAdcClkFreqType[RaonTvChipIdx] )
	{
		case RTV_ADC_CLK_FREQ_8_MHz: 
			RTV_REG_MAP_SEL(COMM_PAGE);
			RTV_REG_SET(0x6A,0x01); 
			   
			RTV_REG_MAP_SEL(OFDM_PAGE);
			RTV_REG_SET(0x3c,0x4B); 
			RTV_REG_SET(0x3d,0x37); 
			RTV_REG_SET(0x3e,0x89); 
			RTV_REG_SET(0x3f,0x41);
			break;
			
		case RTV_ADC_CLK_FREQ_8_192_MHz: 
			RTV_REG_MAP_SEL(COMM_PAGE);
			RTV_REG_SET(0x6A,0x01); 

			RTV_REG_MAP_SEL(OFDM_PAGE);
			RTV_REG_SET(0x3c,0x00); 
			RTV_REG_SET(0x3d,0x00); 
			RTV_REG_SET(0x3e,0x00); 
			RTV_REG_SET(0x3f,0x40);
			break;
			
		case RTV_ADC_CLK_FREQ_9_MHz: 
			RTV_REG_MAP_SEL(COMM_PAGE);
			RTV_REG_SET(0x6A,0x21); 

			RTV_REG_MAP_SEL(OFDM_PAGE);
			RTV_REG_SET(0x3c,0xB5); 
			RTV_REG_SET(0x3d,0x14); 
			RTV_REG_SET(0x3e,0x41); 
			RTV_REG_SET(0x3f,0x3A);
			break;
			
		case RTV_ADC_CLK_FREQ_9_6_MHz:
			RTV_REG_MAP_SEL(COMM_PAGE);
			RTV_REG_SET(0x6A,0x31); 

			RTV_REG_MAP_SEL(OFDM_PAGE);
			RTV_REG_SET(0x3c,0x69); 
			RTV_REG_SET(0x3d,0x03); 
			RTV_REG_SET(0x3e,0x9D); 
			RTV_REG_SET(0x3f,0x36);
			break;
			
		default:
			RTV_DBGMSG0("[tdmb_InitOFDM] Upsupport ADC clock type! \n");
			break;
	}
	
	RTV_REG_SET(0x42,0x00); 
	RTV_REG_SET(0x43,0x00); 

	RTV_REG_SET(0x94,0x08); 

	RTV_REG_SET(0x98,0x05); 
	RTV_REG_SET(0x99,0x03); 
	RTV_REG_SET(0x9B,0xCF); 
	RTV_REG_SET(0x9C,0x10); 
	RTV_REG_SET(0x9D,0x1C); 
	RTV_REG_SET(0x9F,0x32); 
	RTV_REG_SET(0xA0,0x90); 

	RTV_REG_SET(0xA4,0x01); 

	RTV_REG_SET(0xA8,0xF6); 
	RTV_REG_SET(0xA9,0x89);
	RTV_REG_SET(0xAA,0x0C); 
	RTV_REG_SET(0xAB,0x32); 

	RTV_REG_SET(0xAC,0x14); 
	RTV_REG_SET(0xAD,0x09); 

	RTV_REG_SET(0xAE,0xFF); 

    RTV_REG_SET(0xEB,0x6B); 
}

//============================================================================
// Name    : tdmb_InitFEC
// Action  : MAP SEL FEC Register Init
// Input   : Chip Address
// Output  : None
//============================================================================
static void tdmb_InitFEC(void)
{
	RTV_REG_MAP_SEL(FEC_PAGE);

#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2) 
  #ifndef RTV_CIF_MODE_ENABLED /* Individual Mode */
  	RTV_REG_MASK_SET(0x7D, 0x10, 0x10);
  #endif
#endif

	RTV_REG_SET(0x80, 0x80);   
	RTV_REG_SET(0x81, 0xFF);
	RTV_REG_SET(0x87, 0x07);
	RTV_REG_SET(0x45, 0xA0);
	RTV_REG_SET(0xDD, 0xD0); 
	RTV_REG_SET(0x39, 0x07);
	RTV_REG_SET(0xE6, 0x10);    
	RTV_REG_SET(0xA5, 0xA0);
}


static void tdmb_InitDemod(void)
{
	tdmb_InitTOP();
	tdmb_InitCOMM();
	tdmb_InitHOST();
	tdmb_InitOFDM();
	tdmb_InitFEC();	

    rtv_ResetMemory_FIC(); // Must disable before transmit con.
    
    /* Configure interrupt. */
#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2) 
	rtvOEM_ConfigureInterrupt(); 

#elif defined(RTV_IF_MPEG2_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE) || defined(RTV_IF_QUALCOMM_TSIF) || defined(RTV_IF_MPEG2_PARALLEL_TSIF)
  #if (RTV_MAX_NUM_SUB_CHANNEL_USED == 1) /* Single sub channel mode. */
	rtvOEM_ConfigureInterrupt(); 
  #endif
#endif

	/* Configure Individual or CIF mode. */
#ifndef RTV_CIF_MODE_ENABLED /* Individual Mode */
    RTV_REG_MAP_SEL(DD_PAGE);
    RTV_REG_MASK_SET(0x31, 0x03, 0x00); // [0]:MSC0_HEAD_EN, [1]:MSC_HEAD_NBYTE

#else /* CIF Mode */
    RTV_REG_MAP_SEL(DD_PAGE);
    RTV_REG_MASK_SET(0x31, 0x03, 0x03); // [0]:MSC0_HEAD_EN, [1]:MSC_HEAD_NBYTE	
#endif	

	/* Configure TSIF. */
#if defined(RTV_IF_MPEG2_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE) || defined(RTV_IF_QUALCOMM_TSIF)
    rtv_ConfigureTsifFormat(); 
   
	/* Configure TS memory and mode. */
	RTV_REG_MAP_SEL(COMM_PAGE);
  #ifndef RTV_CIF_MODE_ENABLED /* Individual Mode */	
	RTV_REG_SET(0x47, 0x13|RTV_COMM_CON47_CLK_SEL); // MSC1 DD-TSI enable 
  #else  /* CIF Mode */ 
	/* 	
	[5] CIF_MODE_EN: TSI CIF transmit mode enable. 1 = CIF, 0 = Individual
	[4] MSC1_EN: MSC1 transmit enable
	[3] MSC0_EN: MSC0 transmit enable
	[2] FIC_EN: FIC transmit enable  	*/
	RTV_REG_SET(0x47, 0x3F|RTV_COMM_CON47_CLK_SEL); // CIF/FIC/MSC0/MSC1 DD-TSI enable 

    RTV_REG_MAP_SEL(DD_PAGE);
	RTV_REG_SET(0xD6, 0xF4);
  #endif
  
#elif defined(RTV_IF_MPEG2_PARALLEL_TSIF)
  	rtv_SetParallelTsif_TDMB_Only();
#endif		

	rtv_ConfigureHostIF();
}


static void tdmb_SoftReset(void)
{
	RTV_REG_MAP_SEL(OFDM_PAGE);
	RTV_REG_SET(0x10, 0x48); // FEC reset enable
	RTV_DELAY_MS(1);
	RTV_REG_SET(0x10, 0xC9); // OFDM & FEC Soft reset
}


#ifdef RTV_DUAL_CHIP_USED 
void rtvTDMB_EnableDiversity(void)
{
	U8 ofdm_r1a;

	if(g_fRtvDiversityEnabled == TRUE)
		return;

	RTV_GUARD_LOCK;
	
	RTV_MASTER_CHIP_SEL;	
	RTV_REG_MAP_SEL(OFDM_PAGE); 
	
#if defined(RTV_IF_MPEG2_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE) || defined(RTV_IF_QUALCOMM_TSIF) 
	RTV_REG_SET(0x04, 0x69);
#elif defined(RTV_IF_SPI)	
	RTV_REG_SET(0x04, 0x68);
#endif		
	RTV_REG_SET(0x0C, 0xC0);

	RTV_REG_MAP_SEL(COMM_PAGE); 
	RTV_REG_SET(0x10, 0x85);
	RTV_REG_SET(0x11, 0x1E);

	RTV_REG_MAP_SEL(OFDM_PAGE); 
         RTV_REG_SET(0x19, 0x50);   
	ofdm_r1a= RTV_REG_GET(0x1A); 
	RTV_REG_SET(0x1A, (ofdm_r1a | 0x01));

	RTV_SLAVE_CHIP_SEL;
	RTV_REG_MAP_SEL(OFDM_PAGE);
#if defined(RTV_IF_MPEG2_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE) || defined(RTV_IF_QUALCOMM_TSIF) 
	RTV_REG_SET(0x04, 0x69);
#elif defined(RTV_IF_SPI)	
	RTV_REG_SET(0x04, 0x68);
#endif		
	RTV_REG_SET(0x0C, 0xE0);

	RTV_REG_MAP_SEL(COMM_PAGE); 
	RTV_REG_SET(0x10, 0x83);
	RTV_REG_SET(0x11, 0x1E);

	RTV_REG_MAP_SEL(OFDM_PAGE); 
    	RTV_REG_SET(0x19, 0x50);   
	ofdm_r1a= RTV_REG_GET(0x1A); 
	RTV_REG_SET(0x1A, (ofdm_r1a | 0x01));

	g_fRtvDiversityEnabled = TRUE;

	RTV_GUARD_FREE;
}


void rtvTDMB_DisableDiversity(void)
{
	if(g_fRtvDiversityEnabled == FALSE)
		return;
	
	RTV_GUARD_LOCK;
	
	RTV_MASTER_CHIP_SEL;	
	RTV_REG_MAP_SEL(OFDM_PAGE); 

#if defined(RTV_IF_MPEG2_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE) || defined(RTV_IF_QUALCOMM_TSIF) 
	RTV_REG_SET(0x04, 0x29);
	RTV_REG_SET(0x0C, 0xF4);
#elif defined(RTV_IF_SPI)	
	RTV_REG_SET(0x04, 0x40);
	RTV_REG_SET(0x0C, 0xF5);
#endif

	RTV_REG_MAP_SEL(COMM_PAGE); 
	RTV_REG_SET(0x10, 0x91); 
	RTV_REG_SET(0x11, 0x40);

	RTV_REG_MAP_SEL(OFDM_PAGE); 
	RTV_REG_SET(0x19, 0x10);    

	RTV_SLAVE_CHIP_SEL;
	RTV_REG_MAP_SEL(OFDM_PAGE);
#if defined(RTV_IF_MPEG2_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE) || defined(RTV_IF_QUALCOMM_TSIF)
	RTV_REG_SET(0x04, 0x29);
	RTV_REG_SET(0x0C, 0xF4);
#elif defined(RTV_IF_SPI)	
	RTV_REG_SET(0x04, 0x40);
	RTV_REG_SET(0x0C, 0xF5);
#endif		

	RTV_REG_MAP_SEL(COMM_PAGE); 
	RTV_REG_SET(0x10, 0x91); 
	RTV_REG_SET(0x11, 0x40);

	RTV_REG_MAP_SEL(OFDM_PAGE); 
	RTV_REG_SET(0x19, 0x10);    

	g_fRtvDiversityEnabled = FALSE;

	RTV_GUARD_FREE;
}
#endif /* #ifdef RTV_DUAL_CHIP_USED */


void rtvTDMB_StandbyMode(int on)
{
	RTV_GUARD_LOCK;
	
	if( on )
	{ 
		RTV_REG_MAP_SEL(RF_PAGE); 
		RTV_REG_MASK_SET(0x57,0x04, 0x04);  //SW PD ALL      
	}
	else
	{	  
		RTV_REG_MAP_SEL(RF_PAGE); 
		RTV_REG_MASK_SET(0x57,0x04, 0x00);  //SW PD ALL	
	}

	RTV_GUARD_FREE;
}


UINT rtvTDMB_GetLockStatus(void)
{
	U8 lock_stat;
	UINT lock_st = 0;
	
   	if(g_afRtvChannelChange[RaonTvChipIdx]) 
   	{
   		RTV_DBGMSG0("[rtvTDMB_GetLockStatus] RTV Freqency change state! \n");	
		return 0x0;	 
	}

	RTV_GUARD_LOCK;
		
    RTV_REG_MAP_SEL(DD_PAGE);
	lock_stat = RTV_REG_GET(0x37);	
	if( lock_stat & 0x01 )
        lock_st = RTV_TDMB_OFDM_LOCK_MASK;

    RTV_REG_MAP_SEL(FEC_PAGE);
	lock_stat = RTV_REG_GET(0xFB);	

	RTV_GUARD_FREE;
	
	if((lock_stat & 0x03) == 0x03)
        lock_st |= RTV_TDMB_FEC_LOCK_MASK;

	return lock_st;
}


U32 rtvTDMB_GetPER(void)
{
    U8 rdata0, rdata1, rs_sync;

	if(g_afRtvChannelChange[RaonTvChipIdx]) 
	{
		RTV_DBGMSG0("[rtvTDMB_GetPER] RTV Freqency change state! \n");
		return 0;	 
	}	

	RTV_GUARD_LOCK;
	
   	RTV_REG_MAP_SEL(FEC_PAGE);
	rdata0 = RTV_REG_GET(0xD7);

	rs_sync = (rdata0 & 0x08) >> 3;
	if(rs_sync != 0x01)
	{
		RTV_GUARD_FREE;
		return 0;	
	}

	rdata1 = RTV_REG_GET(0xB4);
	rdata0 = RTV_REG_GET(0xB5);

	RTV_GUARD_FREE;
	
	return  ((rdata1 << 8) | rdata0);
}


S32 rtvTDMB_GetRSSI(void) 
{
	U8 RD00;
	S32 nRssi;
	
	if(g_afRtvChannelChange[RaonTvChipIdx]) 
	{
		RTV_DBGMSG0("[rtvTDMB_GetRSSI] RTV Freqency change state! \n");
		return 0;	 
	}	

	RTV_GUARD_LOCK;

	RTV_REG_MAP_SEL(RF_PAGE);	
	RD00 = RTV_REG_GET(0x00); 

	nRssi = -( (((RD00 & 0x30) >> 4) * (S32)(12*RTV_TDMB_RSSI_DIVIDER)) 
			+ ((RD00 & 0x0F) * (S32)(2.8*RTV_TDMB_RSSI_DIVIDER))	
			+ (( (RTV_REG_GET(0x02) & 0x1E) >> 1 ) * (S32)(2.7*RTV_TDMB_RSSI_DIVIDER)) 
			+ ((RTV_REG_GET(0x04) &0x7F) * (S32)(0.4*RTV_TDMB_RSSI_DIVIDER) ) - (S32)(10*RTV_TDMB_RSSI_DIVIDER) ); 

	RTV_GUARD_FREE;
	
	 if((RD00 & 0xC0) == 0x40)
		 nRssi -= 5;

	return  nRssi;
}


U32 rtvTDMB_GetCNR(void)
{
	U8 data1=0, data2=0;
	U8 data=0;
	U32 SNR=0; 

	if(g_afRtvChannelChange[RaonTvChipIdx]) 
	{
		RTV_DBGMSG0("[rtvTDMB_GetCNR] RTV Freqency change state! \n");
		return 0;	 
	}	

	RTV_GUARD_LOCK;

	RTV_REG_MAP_SEL(OFDM_PAGE); 

	RTV_REG_SET(0x82, 0x01);	
	data1 = RTV_REG_GET(0x7E);
	data2 = RTV_REG_GET(0x7F);

	RTV_GUARD_FREE;

	data = ((data2 & 0x1f) << 8) + data1;
	
	if(data == 0)
	{
		return 0;
	}
	else if((data > 0) && (data < 15))
	{
		SNR = (S32)(33 * RTV_TDMB_CNR_DIVIDER);
	}
	else if((data >= 15) && (data <= 160))
	{
		SNR = g_awSNR_15_160[data-15];
	}
	else if(data > 160)
	{
		SNR = (S32)(5.44 * RTV_TDMB_CNR_DIVIDER);
	}

	return SNR;
}

// MSC BER
U32 rtvTDMB_GetCER(void)
{
	U8 rcnt3=0, rcnt2=0, rcnt1=0, rcnt0=0;
	U32 val;

	if(g_afRtvChannelChange[RaonTvChipIdx]) 
	{
		RTV_DBGMSG0("[rtvTDMB_GetBER] RTV Freqency change state! \n");
		return 0;	 
	}	

	RTV_GUARD_LOCK;
	
	RTV_REG_MAP_SEL(FEC_PAGE);
	rcnt3 = RTV_REG_GET(0x8C);
	rcnt2 = RTV_REG_GET(0x8D);
	rcnt1 = RTV_REG_GET(0x8E);
	rcnt0 = RTV_REG_GET(0x8F);

	RTV_GUARD_FREE;
	
	val = (rcnt3 << 24) | (rcnt2 << 16) | (rcnt1 << 8) | rcnt0;

	return ((val / 1000)*25);
}

// Pre BER
U32 rtvTDMB_GetBER(void)
{
	U8 rdata0=0, rdata1=0, rdata2=0;
	U8 rcnt0, rcnt1, rcnt2;
	U8 rs_sync;
	U32 val;
	U32 cnt;

	if(g_afRtvChannelChange[RaonTvChipIdx]) 
	{
		RTV_DBGMSG0("[rtvTDMB_GetBER] RTV Freqency change state! \n");
		return 0;	 
	}	

	RTV_GUARD_LOCK;

	RTV_REG_MAP_SEL(FEC_PAGE);
	rdata0 = RTV_REG_GET(0xD7);

	rs_sync = (rdata0 & 0x08) >> 3;
	if(rs_sync != 0x01)
	{
		RTV_GUARD_FREE;
		return 0;
	}
	
	rcnt2 = RTV_REG_GET(0xA6);
	rcnt1 = RTV_REG_GET(0xA7);
	rcnt0 = RTV_REG_GET(0xA8);
	cnt = (rcnt2 << 16) | (rcnt1 << 8) | rcnt0;

	rdata2 = RTV_REG_GET(0xA9);
	rdata1 = RTV_REG_GET(0xAA);
	rdata0 = RTV_REG_GET(0xAB);
	val = (rdata2 << 16) | (rdata1 << 8) | rdata0; // max 24 bit

	RTV_GUARD_FREE;
	
	if(cnt == 0)
		return 0;
	else
		return ((val * (U32)RTV_TDMB_BER_DIVIDER) / cnt);
}


/* Because that TDMB has the sub channel, we checks the freq which new or the same when the changsing of channel */
U32 rtvTDMB_GetPreviousFrequency(void)
{
	return g_aTdmbPrevChFreqKHz[RaonTvChipIdx];
}

// Interrupts are disabled for SPI
// TSIF stream disabled are for TSIF
static void tdmb_CloseSubChannel(UINT nRegSubChArrayIdx)
{
	UINT nSubChID;
	INT nHwSubChIdx;

	if((g_aRegSubChArrayIdxBits[RaonTvChipIdx] & (1<<nRegSubChArrayIdx)) == 0)
		return; // not opened! already closed!	

	nSubChID  = g_atTdmbRegSubchInfo[RaonTvChipIdx][nRegSubChArrayIdx].nSubChID;
	nHwSubChIdx = g_atTdmbRegSubchInfo[RaonTvChipIdx][nRegSubChArrayIdx].nHwSubChIdx;

	// Disable the specified SUB CH first.
#if (RTV_MAX_NUM_SUB_CHANNEL_USED == 1) /* Single Sub Channel */
	rtv_Set_MSC1_SUBCH0(nSubChID, FALSE, FALSE);	

    #if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
	 RTV_REG_MAP_SEL(HOST_PAGE); 
	 g_abRtvIntrMaskRegL[RaonTvChipIdx] |= MSC1_INTR_BITS;
	 RTV_REG_SET(0x62, g_abRtvIntrMaskRegL[RaonTvChipIdx]); 	 
    #endif			 
	
	rtv_ResetMemory_MSC1(); // Disable MSC1 memory. 
		
#else /* Multi Sub Channel */
	switch( nHwSubChIdx )
	{
		case 0: rtv_Set_MSC1_SUBCH0(nSubChID, FALSE, FALSE); break;
		case 3: rtv_Set_MSC0_SUBCH3(nSubChID, FALSE); break;
		case 4: rtv_Set_MSC0_SUBCH4(nSubChID, FALSE); break;
		case 5: rtv_Set_MSC0_SUBCH5(nSubChID, FALSE); break;
		case 6: rtv_Set_MSC0_SUBCH6(nSubChID, FALSE); break;
		default: break;
	}

	if((g_aRtvUsedHwSubChIdxBits[RaonTvChipIdx] & TDMB_MSC1_SUBCH_USE_MASK) == 0)
	{
	#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
		RTV_REG_MAP_SEL(HOST_PAGE); 
		g_abRtvIntrMaskRegL[RaonTvChipIdx] |= MSC1_INTR_BITS;
		RTV_REG_SET(0x62, g_abRtvIntrMaskRegL[RaonTvChipIdx]);		
	#endif				
		rtv_ResetMemory_MSC1(); // Disable MSC1 memory
	}
	
	if((g_aRtvUsedHwSubChIdxBits[RaonTvChipIdx] & TDMB_MSC0_SUBCH_USE_MASK) == 0)
	{
	#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)	
		RTV_REG_MAP_SEL(HOST_PAGE); 
		g_abRtvIntrMaskRegL[RaonTvChipIdx] |= MSC0_INTR_BITS;
		RTV_REG_SET(0x62, g_abRtvIntrMaskRegL[RaonTvChipIdx]);		
	#endif				
		rtv_ResetMemory_MSC0(); // Disable MSC0 memory
	}

   #ifdef RTV_CIF_MODE_ENABLED
	rtvCIFDEC_DeleteSubChannelID(nSubChID);
   #endif
#endif	

	/* Delete a used sub channel index. */
	g_aRtvUsedHwSubChIdxBits[RaonTvChipIdx] &= ~(1<<nHwSubChIdx);

	/* Delete a registered subch array index. */
	g_aRegSubChArrayIdxBits[RaonTvChipIdx] &= ~(1<<nRegSubChArrayIdx);

	/* Deregister a sub channel ID Bit. */
	g_aRegSubChIdBits[RaonTvChipIdx][SUBCH_GRP_IDX(nSubChID)] &= ~(1 << MOD32(nSubChID)); 
}


#if (RTV_MAX_NUM_SUB_CHANNEL_USED > 1)
static void tdmb_CloseAllSubChannel(void)
{
	UINT i = 0;
	UINT nRegSubChArrayIdxBits = g_aRegSubChArrayIdxBits[RaonTvChipIdx];
	
	while(nRegSubChArrayIdxBits != 0)
	{
		if( nRegSubChArrayIdxBits & 0x01 ) 	
		{
			tdmb_CloseSubChannel(i);		
		}		

		nRegSubChArrayIdxBits >>= 1;
		i++;
	}
}
#endif



INT rtvTDMB_CloseSubChannel(UINT nSubChID)
{
#if (RTV_MAX_NUM_SUB_CHANNEL_USED > 1)
	UINT i = 0;
	UINT nRegSubChArrayIdxBits = g_aRegSubChArrayIdxBits[RaonTvChipIdx];
#endif

	RTV_GUARD_LOCK;

//	rtv_StreamDisable(RTV_TV_MODE_TDMB);

#if (RTV_MAX_NUM_SUB_CHANNEL_USED == 1) /* Single Sub Channel */
	tdmb_CloseSubChannel(0);
#else
	
	while(nRegSubChArrayIdxBits != 0)
	{
		if( nRegSubChArrayIdxBits & 0x01 )	
		{
			if(nSubChID == g_atTdmbRegSubchInfo[RaonTvChipIdx][i].nSubChID)
				tdmb_CloseSubChannel(i);		
		}		

		nRegSubChArrayIdxBits >>= 1;
		i++;
	}
#endif	

//	rtv_StreamRestore(RTV_TV_MODE_TDMB);
	
	RTV_GUARD_FREE;
			
	return RTV_SUCCESS;
}



static void tdmb_OpenSubChannel(UINT nSubChID, E_RTV_TDMB_SERVICE_TYPE eServiceType, UINT nThresholdSize)
{
	INT nHwSubChIdx;
	UINT i = 0;
	
#if (RTV_MAX_NUM_SUB_CHANNEL_USED == 1) /* Single Subchannel */
	nHwSubChIdx = 0;		

	/* Set the sub-channel and enable MSC memory with the specified sub ID. */			
	if(eServiceType == RTV_TDMB_SERVICE_VIDEO)
		rtv_Set_MSC1_SUBCH0(nSubChID, TRUE, TRUE); // RS enable
	else
		rtv_Set_MSC1_SUBCH0(nSubChID, TRUE, FALSE); // RS Disable
			
   #if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)	
  	RTV_REG_MAP_SEL(DD_PAGE);  	
       RTV_REG_SET(INT_E_UCLRL, 0x04); // MSC1 Interrupt status clear.

  	RTV_REG_MAP_SEL(HOST_PAGE);	
  	g_abRtvIntrMaskRegL[RaonTvChipIdx] &= ~(MSC1_INTR_BITS);
  	RTV_REG_SET(0x62, g_abRtvIntrMaskRegL[RaonTvChipIdx]); /* Enable MSC1 interrupts. */ 	
   #endif

  	rtv_SetupMemory_MSC1(nThresholdSize);	
  
#else /* Multi sub channel enabled */	

	if(eServiceType == RTV_TDMB_SERVICE_VIDEO)
	{
		nHwSubChIdx = 0;		

		rtv_Set_MSC1_SUBCH0(nSubChID, TRUE, TRUE); // RS enable

		if((g_aRtvUsedHwSubChIdxBits[RaonTvChipIdx] & TDMB_MSC1_SUBCH_USE_MASK) == 0)
		{	/* First enabled. */
		#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
			RTV_REG_MAP_SEL(DD_PAGE);
			RTV_REG_SET(INT_E_UCLRL, 0x04); // MSC1 Interrupt status clear.

			RTV_REG_MAP_SEL(HOST_PAGE);	
			g_abRtvIntrMaskRegL[RaonTvChipIdx] &= ~(MSC1_INTR_BITS);
			RTV_REG_SET(0x62, g_abRtvIntrMaskRegL[RaonTvChipIdx]); /* Enable MSC1 interrupts and restore FIC . */		
		#endif
			rtv_SetupMemory_MSC1(nThresholdSize); 
		}	

    #if defined(RTV_IF_MPEG2_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE) || defined(RTV_IF_QUALCOMM_TSIF)			
	#ifdef RTV_CIF_MODE_ENABLED
	 	rtvCIFDEC_AddSubChannelID(nSubChID);	
	#endif	
   #endif
	}
	else /* Audio/Data */
	{
		/* Search an available SUBCH index for Audio/Data service. (3 ~ 6) */
		for(nHwSubChIdx=3/* MSC0 base */; nHwSubChIdx<=6; nHwSubChIdx++)
		{
			if((g_aRtvUsedHwSubChIdxBits[RaonTvChipIdx] & (1<<nHwSubChIdx)) == 0) 			
				break;			
		}			 	
		
		/* Set sub channel. */ 
		switch( nHwSubChIdx )
		{	
			case 3: rtv_Set_MSC0_SUBCH3(nSubChID, TRUE); break;
			case 4: rtv_Set_MSC0_SUBCH4(nSubChID, TRUE); break;
			case 5: rtv_Set_MSC0_SUBCH5(nSubChID, TRUE); break;
			case 6: rtv_Set_MSC0_SUBCH6(nSubChID, TRUE); break;
			default: break;
		}

		if((g_aRtvUsedHwSubChIdxBits[RaonTvChipIdx] & TDMB_MSC0_SUBCH_USE_MASK) == 0)
		{
	#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
			RTV_REG_MAP_SEL(DD_PAGE);
			RTV_REG_SET(INT_E_UCLRL,0x02); // MSC0 Interrupt clear.			

			/* Enable MSC0 interrupts. */
			RTV_REG_MAP_SEL(HOST_PAGE);	
			g_abRtvIntrMaskRegL[RaonTvChipIdx] &= ~(MSC0_INTR_BITS);
			RTV_REG_SET(0x62, g_abRtvIntrMaskRegL[RaonTvChipIdx]);	 // restore FIC	
	#endif				
			rtv_SetupMemory_MSC0(nThresholdSize); /* CIF mode. */
		}

	#ifdef RTV_CIF_MODE_ENABLED
	 	rtvCIFDEC_AddSubChannelID(nSubChID);	 
	#endif
	}	
#endif 		

	/* To use when close .*/	
#if (RTV_MAX_NUM_SUB_CHANNEL_USED > 1)
	for(i=0; i<RTV_MAX_NUM_SUB_CHANNEL_USED; i++)
	{
		if((g_aRegSubChArrayIdxBits[RaonTvChipIdx] & (1<<i)) == 0)		
		{
#else
	i = 0;
#endif		
			/* Register a array index of sub channel */
			g_aRegSubChArrayIdxBits[RaonTvChipIdx] |= (1<<i);
			
			g_atTdmbRegSubchInfo[RaonTvChipIdx][i].nSubChID = nSubChID;

			/* Add the new sub channel index. */
			g_atTdmbRegSubchInfo[RaonTvChipIdx][i].nHwSubChIdx  = nHwSubChIdx;
			g_atTdmbRegSubchInfo[RaonTvChipIdx][i].eServiceType   = eServiceType;
			g_atTdmbRegSubchInfo[RaonTvChipIdx][i].nThresholdSize = nThresholdSize;
#if (RTV_MAX_NUM_SUB_CHANNEL_USED > 1)			
			break;
		}
	}		
#endif	

	/* Add the HW sub channel index. */
	g_aRtvUsedHwSubChIdxBits[RaonTvChipIdx] |= (1<<nHwSubChIdx);	

	/* Register a new sub channel ID Bit. */
	g_aRegSubChIdBits[RaonTvChipIdx][SUBCH_GRP_IDX(nSubChID)] |= (1 << MOD32(nSubChID)); 

	//RTV_DBGMSG2("[tdmb_OpenSubChannel] nSubChID(%d) use MSC_SUBCH%d\n", nSubChID, nHwSubChIdx);	
}


INT rtvTDMB_OpenSubChannel(U32 dwChFreqKHz, UINT nSubChID, 
								    E_RTV_TDMB_SERVICE_TYPE eServiceType, UINT nThresholdSize)
{
	INT nRet = RTV_SUCCESS;
	
#if defined(RTV_CIF_MODE_ENABLED) || defined(RTV_IF_MPEG2_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE) || defined(RTV_IF_QUALCOMM_TSIF) || defined(RTV_IF_MPEG2_PARALLEL_TSIF)	
	nThresholdSize = RTV_TS_PACKET_SIZE;
#endif

	if(nSubChID > (MAX_NUM_TDMB_SUB_CH-1))
		return RTV_INVAILD_SUB_CHANNEL_ID;

	/* Check for threshold size. */
#ifndef RTV_CIF_MODE_ENABLED /* Individual Mode */
   #if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
	 if(nThresholdSize > (188*18))  
	 	return RTV_INVAILD_THRESHOLD_SIZE;
   #endif
#endif

	/* Check the previous freq. */
	if(g_aTdmbPrevChFreqKHz[RaonTvChipIdx] == dwChFreqKHz)
	{
		/* Is registerd sub ch ID? */
		if(g_aRegSubChIdBits[RaonTvChipIdx][SUBCH_GRP_IDX(nSubChID)] & (1<<MOD32(nSubChID)))
		{
			RTV_GUARD_LOCK;
			rtv_StreamRestore(RTV_TV_MODE_TDMB);// To restore FIC stream.
			RTV_GUARD_FREE;

			RTV_DBGMSG1("[rtvTDMB_OpenSubChannel] Already opened sub channed ID(%d)\n", nSubChID);
			
			return RTV_SUCCESS;
		}
				
   #if (RTV_MAX_NUM_SUB_CHANNEL_USED == 1)  /* Single Sub Channel Mode */
   		RTV_GUARD_LOCK;
   		tdmb_CloseSubChannel(0); /* Max sub channel is 1. So, we close the previous sub ch. */
   		tdmb_OpenSubChannel(nSubChID, eServiceType, nThresholdSize);
		RTV_GUARD_FREE;
   #else
	/* Multi Sub Channel. */
		if(eServiceType == RTV_TDMB_SERVICE_VIDEO)
		{
			/* Check if the SUBCH available for Video service ? */
			if((g_aRtvUsedHwSubChIdxBits[RaonTvChipIdx] & TDMB_MSC1_SUBCH_USE_MASK) == TDMB_MSC1_SUBCH_USE_MASK)
			{
				RTV_GUARD_LOCK;
				rtv_StreamRestore(RTV_TV_MODE_TDMB);// To restore FIC stream.
				RTV_GUARD_FREE;

				return RTV_NO_MORE_SUB_CHANNEL; // Only 1 Video service. 
			}
		}
		else /* Audio/Data */
		{
			/* Check if the SUBCH available for Audio/Data services for MSC0 ? */
			if((g_aRtvUsedHwSubChIdxBits[RaonTvChipIdx] & TDMB_MSC0_SUBCH_USE_MASK) == TDMB_MSC0_SUBCH_USE_MASK)
			{
				RTV_GUARD_LOCK;
				rtv_StreamRestore(RTV_TV_MODE_TDMB);// To restore FIC stream.
				RTV_GUARD_FREE;

				return RTV_NO_MORE_SUB_CHANNEL; // Not available SUBCH for Audio/Data.
			}
		}   

		RTV_GUARD_LOCK;
		tdmb_OpenSubChannel(nSubChID, eServiceType, nThresholdSize);    
		rtv_StreamRestore(RTV_TV_MODE_TDMB);// To restore FIC stream. This is NOT the first time.
		RTV_GUARD_FREE;
   #endif
	}/* if(g_aTdmbPrevChFreqKHz[RaonTvChipIdx] == dwChFreqKHz) */
	else
	{
		g_aTdmbPrevChFreqKHz[RaonTvChipIdx] = dwChFreqKHz;

		g_afRtvChannelChange[RaonTvChipIdx] = TRUE; // To prevent get ber, cnr ...
		
		RTV_GUARD_LOCK;

#if (RTV_MAX_NUM_SUB_CHANNEL_USED == 1)
		tdmb_CloseSubChannel(0); // Cloes the previous sub channel because this channel is a new freq.
#else	         
		tdmb_CloseAllSubChannel(); // Cloes the all sub channel because this channel is a new freq.	  
#endif	
				   
		nRet = rtvRF_SetFrequency(RTV_TV_MODE_TDMB, 0, dwChFreqKHz);

  		tdmb_OpenSubChannel(nSubChID, eServiceType, nThresholdSize);
	
		RTV_GUARD_FREE;

		g_afRtvChannelChange[RaonTvChipIdx] = FALSE; 
	}

	return nRet;
}


/*
	_rtvTDMB_ReadFIC()
	
	This function reads a FIC data in manually. 
*/
UINT rtvTDMB_ReadFIC(U8 *pbBuf)
{
#ifdef RTV_FIC_POLLING_MODE	
	U8 int_type_val1;	
	UINT cnt = 0;	
	U8 tr_mode;
	const UINT frame_dur[4] = {(96)/2, (96/4)/2, (96/4)/2, (96/2)/2};
	
	if(g_afRtvTdmbConfiguredFIC[RaonTvChipIdx] == FALSE)
	{
		RTV_DBGMSG0("[rtvTDMB_ReadFIC] NOT OPEN FIC\n");
		return 0;
	}

	RTV_GUARD_LOCK;
	
	RTV_REG_MAP_SEL(0x06);
	tr_mode = RTV_REG_GET(0x27);	// DAB TX Mode monitoring
	tr_mode = (tr_mode & 0x30)>>4;

	RTV_REG_MAP_SEL(DD_PAGE);	
	while(++cnt <= 10)
	{
		int_type_val1 = RTV_REG_GET(INT_E_STATL);
		if(int_type_val1 & FIC_E_INT) // FIC interrupt
		{
		    //printk("FIC_E_INT occured!\n");

			RTV_REG_MAP_SEL(FIC_PAGE);
	#if defined(RTV_IF_MPEG2_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE) || defined(RTV_IF_QUALCOMM_TSIF) || defined(RTV_IF_MPEG2_PARALLEL_TSIF)
			RTV_REG_BURST_GET(0x10, pbBuf, 192);			
			RTV_REG_BURST_GET(0x10, pbBuf+192, 192);	

	#elif defined(RTV_IF_SPI) 
		#if defined(__KERNEL__)
			RTV_REG_BURST_GET(0x10, pbBuf, 384+1);
		#else
			RTV_REG_BURST_GET(0x10, pbBuf, 384);
		#endif
	#endif	
			RTV_REG_MAP_SEL(DD_PAGE);
			RTV_REG_SET(INT_E_UCLRL, 0x01); // FIC interrupt status clear

			RTV_GUARD_FREE;

			return 384; 

		}
		RTV_DELAY_MS(frame_dur[tr_mode]);
	} /* while() */	

	RTV_GUARD_FREE;

	RTV_DBGMSG0("[rtvTDMB_ReadFIC] FIC read timeout\n");

	return 0;

#else
		RTV_GUARD_LOCK;

		RTV_REG_MAP_SEL(FIC_PAGE);
	#if defined(RTV_IF_MPEG2_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE) || defined(RTV_IF_QUALCOMM_TSIF) || defined(RTV_IF_MPEG2_PARALLEL_TSIF)
		RTV_REG_BURST_GET(0x10, pbBuf, 192);			
		RTV_REG_BURST_GET(0x10, pbBuf+192, 192);	

	#elif defined(RTV_IF_SPI) 
		#if defined(__KERNEL__)
		RTV_REG_BURST_GET(0x10, pbBuf, 384+1);
		#else
		RTV_REG_BURST_GET(0x10, pbBuf, 384);
		#endif
	#endif	
	
		RTV_REG_MAP_SEL(DD_PAGE);
		RTV_REG_SET(INT_E_UCLRL, 0x01); // FIC interrupt status clear

		RTV_GUARD_FREE;
	
		return 384;	
#endif		
}


void rtvTDMB_CloseFIC(void)
{
	if(g_afRtvTdmbConfiguredFIC[RaonTvChipIdx] == FALSE)
		return;

	RTV_GUARD_LOCK;
		
#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2) 
  #ifndef RTV_FIC_POLLING_MODE
	/* Disable the FIC interrupt. */
	RTV_REG_MAP_SEL(HOST_PAGE);
	g_abRtvIntrMaskRegL[RaonTvChipIdx] |= FIC_E_INT;
	RTV_REG_SET(0x62, g_abRtvIntrMaskRegL[RaonTvChipIdx]);     
  #endif	

#elif defined(RTV_IF_MPEG2_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE) || defined(RTV_IF_QUALCOMM_TSIF) || defined(RTV_IF_MPEG2_PARALLEL_TSIF)
  #if (RTV_MAX_NUM_SUB_CHANNEL_USED == 1) /* Single sub channel mode */
    #ifndef RTV_FIC_POLLING_MODE
	/* Disable the FIC interrupt. */
	RTV_REG_MAP_SEL(HOST_PAGE);
	g_abRtvIntrMaskRegL[RaonTvChipIdx] |= FIC_E_INT;
	RTV_REG_SET(0x62, g_abRtvIntrMaskRegL[RaonTvChipIdx]);	  
   #endif
  #endif
#endif

	rtv_ResetMemory_FIC();

	g_afRtvTdmbConfiguredFIC[RaonTvChipIdx] = FALSE;

	RTV_GUARD_FREE;
}


void rtvTDMB_OpenFIC(void)
{
	if(g_afRtvTdmbConfiguredFIC[RaonTvChipIdx] == TRUE)
		return;
		
	g_afRtvTdmbConfiguredFIC[RaonTvChipIdx] = TRUE;

	RTV_GUARD_LOCK;

#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)	
  #ifndef RTV_FIC_POLLING_MODE
    	RTV_REG_MAP_SEL(DD_PAGE);
    	RTV_REG_SET(0x35, 0x01); // FIC Interrupt status clear.	

	/* Enable the FIC interrupt. */
	RTV_REG_MAP_SEL(HOST_PAGE);
	g_abRtvIntrMaskRegL[RaonTvChipIdx] &= ~(FIC_E_INT);
    	RTV_REG_SET(0x62, g_abRtvIntrMaskRegL[RaonTvChipIdx]); 
  #endif
    
#elif defined(RTV_IF_MPEG2_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE) || defined(RTV_IF_QUALCOMM_TSIF) || defined(RTV_IF_MPEG2_PARALLEL_TSIF)	
  #if (RTV_MAX_NUM_SUB_CHANNEL_USED == 1) /* Single sub channel mode */
    #ifndef RTV_FIC_POLLING_MODE
    	RTV_REG_MAP_SEL(DD_PAGE);
    	RTV_REG_SET(0x35, 0x01); // FIC Interrupt status clear.	

	/* Enable the FIC interrupt. This mode only use FIC interrupt. MSC used by TSIF. */
	RTV_REG_MAP_SEL(HOST_PAGE);
	g_abRtvIntrMaskRegL[RaonTvChipIdx] &= ~(FIC_E_INT);
    	RTV_REG_SET(0x62, g_abRtvIntrMaskRegL[RaonTvChipIdx]); 
   #endif	
  #endif
#endif	    

	rtv_SetupMemory_FIC(); // Enable FIC memory.

	RTV_GUARD_FREE;
}


/* When this function was called, all sub channel should closed to reduce scan-time !!!!! 
   FIC can enabled. Usally enabled.
 */
INT rtvTDMB_ScanFrequency(U32 dwChFreqKHz)
{
	U8 scan_done, OFDM_L=0, ccnt = 0, NULL_C=0, SCV_C=0;
	U8 scan_pwr1=0, scan_pwr2=0, DAB_Mode=0xFF, DAB_Mode_Chk=0xFF;
	U8 pre_agc1=0, pre_agc2=0, pre_agc_mon=0, ASCV=0;
	INT scan_flag = 0;
	U16 SPower =0, PreGain=0, PreGainTH=0, PWR_TH = 0, ILoopTH =0; 
	U8 Cfreq_HTH = 0,Cfreq_LTH=0;
	U8 i=0,j=0, m=0,n=0;
	U8 varyLow=0,varyHigh=0;
	U16 varyMon=0;
	U8 MON_FSM=0, FsmCntChk=0;
	U8 test0=0,test1=0;
	U16 NullLengthMon=0;
	U16 fail = 0;
	U8  FecResetCh=0xff;
	U8 FEC_SYNC=0xFF,CoarseFreq=0xFF, NullTh=0xFF,NullChCnt=0;	
	U8 rdata0 =0, rdata1=0;
	U16 i_chk=0, q_chk=0;
	UINT nReTryCnt = 0;

	g_afRtvChannelChange[RaonTvChipIdx] = TRUE; // To prevent get ber, cnr ...

	RTV_GUARD_LOCK;

	/* NOTE: When this rountine executed, all sub channel should closed 
			 and memory(MSC0, MSC1) interrupts are disabled. */
#if (RTV_MAX_NUM_SUB_CHANNEL_USED == 1)
	tdmb_CloseSubChannel(0);
#else	         
	tdmb_CloseAllSubChannel();	  
#endif	
	
	scan_flag = rtvRF_SetFrequency(RTV_TV_MODE_TDMB, 0, dwChFreqKHz);
	if(scan_flag != RTV_SUCCESS)
		goto TDMB_SCAN_EXIT;

	RTV_REG_MAP_SEL(OFDM_PAGE);
	RTV_REG_SET( 0x54, 0x70); 

	tdmb_SoftReset();
		
	FecResetCh = 0xff;
	fail = 0xFFFF;

	while(1)
	{			
		if(++nReTryCnt == 10000) /* Up to 400ms */
		{
			RTV_DBGMSG0("[rtvTDMB_ScanFrequency] Scan Timeout! \n");
			scan_flag = RTV_CHANNEL_NOT_DETECTED;
			break;
		}
			
		RTV_REG_MAP_SEL(OFDM_PAGE);
		scan_done = RTV_REG_GET(0xCF); // Scan-done flag & scan-out flag check

		RTV_REG_MAP_SEL(COMM_PAGE);		    // Scan-Power monitoring
		scan_pwr1 = RTV_REG_GET(0x38);
		scan_pwr2 = RTV_REG_GET(0x39);
		SPower = (scan_pwr2<<8)|scan_pwr1;

		RTV_REG_MAP_SEL(OFDM_PAGE);

		if(scan_done != 0xff)
		{
			NULL_C = 0;
			SCV_C = 0;
			pre_agc_mon = RTV_REG_GET(0x53);
			RTV_REG_SET(0x53, (pre_agc_mon | 0x80));		// Pre-AGC Gain monitoring One-shot
			pre_agc1 = RTV_REG_GET(0x66);
			pre_agc2 = RTV_REG_GET(0x67);
			PreGain = (pre_agc2<<2)|(pre_agc1&0x03);

			DAB_Mode = RTV_REG_GET(0x27);	// DAB TX Mode monitoring
			DAB_Mode = (DAB_Mode & 0x30)>>4;

			switch( g_aeRtvAdcClkFreqType[RaonTvChipIdx] )
			{
				case RTV_ADC_CLK_FREQ_8_MHz :
					PreGainTH = 405;					
					switch(DAB_Mode) // tr mode
					{
				              case 0:
							PWR_TH = 2400;
							ILoopTH = 200;
							Cfreq_HTH = 206;
							Cfreq_LTH = 55;
							break;
						case 1:
							PWR_TH = 2000;
							ILoopTH = 180;
							Cfreq_HTH = 242;
							Cfreq_LTH = 14;
							break;
						case 2:
							PWR_TH = 1300;
							ILoopTH = 180;
							Cfreq_HTH = 248;
							Cfreq_LTH = 8;
							break;
						case 3:
							PWR_TH = 280;
							ILoopTH = 180;
							Cfreq_HTH = 230;
							Cfreq_LTH = 26;
							break;
						default:
							PWR_TH = 2400;
							ILoopTH = 200;
							Cfreq_HTH = 206;
							Cfreq_LTH = 55;
							break;
					}
					break;

				case RTV_ADC_CLK_FREQ_8_192_MHz :
					PreGainTH = 405;
					
					switch(DAB_Mode)
					{
		                               case 0:
							PWR_TH = 1700;
							ILoopTH = 200;
							Cfreq_HTH = 206;
							Cfreq_LTH = 55;
							break;
						case 1:
							PWR_TH = 1500;
							ILoopTH = 180;
							Cfreq_HTH = 242;
							Cfreq_LTH = 14;
							break;
						case 2:
							PWR_TH = 1200;
							ILoopTH = 180;
							Cfreq_HTH = 248;
							Cfreq_LTH = 8;
							break;
						case 3:
							PWR_TH = 1900;
							ILoopTH = 180;
							Cfreq_HTH = 230;
							Cfreq_LTH = 26;
							break;
						default:
							PWR_TH = 1700;
							ILoopTH = 200;
							Cfreq_HTH = 206;
							Cfreq_LTH = 55;
							break;
					}				
					break;

				case RTV_ADC_CLK_FREQ_9_MHz :
					PreGainTH = 380;
					switch(DAB_Mode)
					{
		                                case 0:
							PWR_TH = 7000;
							ILoopTH = 200;
							Cfreq_HTH = 206;
							Cfreq_LTH = 55;
							break;
						case 1:
							PWR_TH = 5000;
							ILoopTH = 180;
							Cfreq_HTH = 242;
							Cfreq_LTH = 14;
							break;
						case 2:
							PWR_TH = 1300;
							ILoopTH = 180;
							Cfreq_HTH = 248;
							Cfreq_LTH = 8;
							break;
						case 3:
							PWR_TH = 8000;
							ILoopTH = 180;
							Cfreq_HTH = 230;
							Cfreq_LTH = 26;
							break;
						default:
							PWR_TH = 8000;
							ILoopTH = 200;
							Cfreq_HTH = 206;
							Cfreq_LTH = 55;
							break;
					}
					break;

				case RTV_ADC_CLK_FREQ_9_6_MHz :
					PreGainTH = 380;
					
					switch(DAB_Mode)
					{
		                                case 0:
							PWR_TH = 7000;
							ILoopTH = 200;
							Cfreq_HTH = 206;
							Cfreq_LTH = 55;
							break;
						case 1:
							PWR_TH = 5000;
							ILoopTH = 180;
							Cfreq_HTH = 242;
							Cfreq_LTH = 14;
							break;
						case 2:
							PWR_TH = 1300;
							ILoopTH = 180;
							Cfreq_HTH = 248;
							Cfreq_LTH = 8;
							break;
						case 3:
							PWR_TH = 8000;
							ILoopTH = 180;
							Cfreq_HTH = 230;
							Cfreq_LTH = 26;
							break;
						default:
							PWR_TH = 8000;
							ILoopTH = 200;
							Cfreq_HTH = 206;
							Cfreq_LTH = 55;
							break;
					}
				
					break;

				default:
					scan_flag = RTV_UNSUPPORT_ADC_CLK;
					goto TDMB_SCAN_EXIT;
			}

			if(scan_done == 0x01)			 /* Not DAB signal channel */
			{
				scan_flag = RTV_CHANNEL_NOT_DETECTED;
				fail = 0xEF01;	
				
				goto TDMB_SCAN_EXIT;		/* Auto-scan result return */
			}
			else if(scan_done == 0x03)	/* DAB signal channel */
			{
				RTV_REG_MAP_SEL(OFDM_PAGE); 
				CoarseFreq = RTV_REG_GET( 0x18);	/* coarse freq */
						
			if(g_eRtvCountryBandType == RTV_COUNTRY_BAND_KOREA)
			{
				if(DAB_Mode > 0)   // Tr_mode detection miss for T-DMB [Only Tr_Mode 1�� ��쿡�� ��밡���� ����] 
				{
					scan_flag = RTV_CHANNEL_NOT_DETECTED;
					fail = 0xE002;

					//RTV_DBGMSG1("[_rtvTDMB_ScanFrequency_DAB Fail] %d\n", DAB_Mode);
					goto TDMB_SCAN_EXIT;	 //Auto-scan result return  
				}
			}
			
				if((CoarseFreq < Cfreq_HTH) && (CoarseFreq  > Cfreq_LTH))
				{
					CoarseFreq = 0x33;					
					scan_flag = RTV_CHANNEL_NOT_DETECTED;
					fail = 0xEF33;	
					goto TDMB_SCAN_EXIT;					
				}	
				
				if(SPower<PWR_TH)  /* Scan Power Threshold	*/
				{		
					scan_flag = RTV_CHANNEL_NOT_DETECTED;
					fail = 0xEF03;	
					goto TDMB_SCAN_EXIT;  
				}
				else
				{
					if ((PreGain < PreGainTH)||(PreGain==0))   /* PreAGC Gain threshold check */
					{
						scan_flag = RTV_CHANNEL_NOT_DETECTED;
						fail = 0xEF04;	 	
						goto TDMB_SCAN_EXIT;
					}
					else
					{
						for(m =0; m<16; m++)
						{
							NullTh = RTV_REG_GET( 0x1C);
							RTV_REG_SET( 0x1C, (NullTh | 0x10));	
							test0 = RTV_REG_GET( 0x26);
							test1 = RTV_REG_GET( 0x27);
							NullLengthMon = ((test1&0x0F)<<8)|test0;
							
							DAB_Mode_Chk = RTV_REG_GET( 0x27);	 /* DAB TX Mode monitoring */
							DAB_Mode_Chk = (DAB_Mode_Chk & 0x30)>>4;				
							if(DAB_Mode != DAB_Mode_Chk)
							{
								scan_flag = RTV_CHANNEL_NOT_DETECTED;
								fail = 0xE000;	
								goto TDMB_SCAN_EXIT; 
							}
							
							if((NullLengthMon == 0) || (NullLengthMon > 3000))
							{
								NullChCnt++;
							}
							if((NullChCnt > 10) && (m > 14)&& (PreGain < 400))	
							{
								scan_flag = RTV_CHANNEL_NOT_DETECTED;
								fail = 0xEF05;	
								goto TDMB_SCAN_EXIT;							
							}
							else if(m>14)
							{
								fail = 0x1111;	
								scan_flag=RTV_SUCCESS;
								break;
							}							
							RTV_DELAY_MS(10);	/* 10ms�� ������� �� */
						}
					}
					if(scan_flag == RTV_SUCCESS)
					{						
						for(i=0; i</*ILoopTH*/100; i++)
						{
							RTV_DELAY_MS(10);	/* 10ms�� ������� �� */
							
							RTV_REG_MAP_SEL(OFDM_PAGE);
							ASCV = RTV_REG_GET( 0x30);
							ASCV = ASCV&0x0F;											
							if((SCV_C > 0) && (ASCV > 7))		  // ASCV count
							{
								scan_flag = RTV_CHANNEL_NOT_DETECTED;
								fail = 0xFF08;	
								goto TDMB_SCAN_EXIT;  /* Auto-scan result return */
							}								
							if(ASCV > 7)
							{
								SCV_C++;		
							}						
							
							DAB_Mode_Chk = RTV_REG_GET( 0x27);	 /* DAB TX Mode monitoring */
							DAB_Mode_Chk = (DAB_Mode_Chk & 0x30)>>4;				
							if(DAB_Mode != DAB_Mode_Chk)
							{
								scan_flag = RTV_CHANNEL_NOT_DETECTED;
								fail = 0xF100;	
								goto TDMB_SCAN_EXIT; 
							}							

							RTV_REG_MAP_SEL( COMM_PAGE); 
							RTV_REG_MASK_SET(0x4D, 0x04, 0x00); 
							RTV_REG_MASK_SET(0x4D, 0x04, 0x04); 
							rdata0 = RTV_REG_GET( 0x4E);
							rdata1 = RTV_REG_GET( 0x4F);
							i_chk = (rdata1 << 8) + rdata0;
							
							rdata0 = RTV_REG_GET( 0x50);
							rdata1 = RTV_REG_GET( 0x51);
							q_chk = (rdata1 << 8) + rdata0; 
							if((((i_chk>5) && (i_chk<65530)) || ((q_chk>5) && (q_chk<65530))) && (PreGain<500))
							{
								scan_flag = RTV_CHANNEL_NOT_DETECTED;
								fail = 0xF200;	
								goto TDMB_SCAN_EXIT; 
							}

							/* //////////////////////// FSM Monitoring check//////////////////////////////// */
							RTV_REG_MAP_SEL(OFDM_PAGE); 
							MON_FSM = RTV_REG_GET( 0x37);
							MON_FSM = MON_FSM & 0x07;

							if((MON_FSM == 1) && (PreGain < 600))	
							{
								FsmCntChk++;
								if(NullChCnt > 14)
									FsmCntChk += 3;
							}
							if((MON_FSM == 1) && (FsmCntChk > 9) && (ccnt < 2))
							{
								scan_flag = RTV_CHANNEL_NOT_DETECTED;									
								fail = 0xFF0A;	
								FsmCntChk = 0;
								
								goto TDMB_SCAN_EXIT;	/* Auto-scan result return */
							}	
							/* /////////////////////////////////////////////////////////////////////////////// */
							/* ///////////////////////// Coarse Freq. check/////////////////////////////////// */
							/* /////////////////////////////////////////////////////////////////////////////// */
							ccnt = RTV_REG_GET( 0x17);	/* Coarse count check */
							ccnt &= 0x1F;
							if(ccnt > 1)
							{
								for(j=0;j<50;j++)
								{
									RTV_DELAY_MS(10);	/* 5ms�� ������� �� */
									RTV_REG_MAP_SEL( OFDM_PAGE);	
									OFDM_L = RTV_REG_GET( 0x12);
									RTV_REG_MASK_SET(0x82, 0x01, 0x01);	
									varyLow = RTV_REG_GET( 0x7E);
									varyHigh = RTV_REG_GET( 0x7F);			   
									varyMon = ((varyHigh & 0x1f) << 8) + varyLow; 
									if((OFDM_L&0x80) && (varyMon > 0))
									{
										RTV_REG_MAP_SEL(OFDM_PAGE);
										RTV_REG_SET(0x54,0x58); 
										break;
									}
								}
								if(OFDM_L&0x80)
								{
									RTV_REG_MAP_SEL( FEC_PAGE);
									FEC_SYNC = RTV_REG_GET( 0xFB);
									FEC_SYNC = FEC_SYNC & 0x03;
									if(FEC_SYNC == 0x03)
									{
										scan_flag = RTV_SUCCESS;	   /* OFDM_Lock & FEC_Sync OK */
										fail = 0xFF70;	
										goto TDMB_SCAN_EXIT;
									}
									else if(FEC_SYNC == 0x02)
									{
										for(n=0;n<20;n++)
										{
											RTV_DELAY_MS(10);	/* 5ms�� ������� �� */
											RTV_REG_MAP_SEL( FEC_PAGE);
											FEC_SYNC = RTV_REG_GET( 0xFB);
											FEC_SYNC = FEC_SYNC & 0x03;
											RTV_REG_MAP_SEL( OFDM_PAGE);
											OFDM_L = RTV_REG_GET( 0x12);
											OFDM_L = OFDM_L & 0x80;
											if(OFDM_L == 0x80)
											{
												if(FEC_SYNC == 0x03)
												{
													scan_flag = RTV_SUCCESS;
													fail = 0xFF71;	  /* FEC_Sync OK */ 								
													goto TDMB_SCAN_EXIT;
												}
												else if(FEC_SYNC == 0x02)
												{
													scan_flag = RTV_CHANNEL_NOT_DETECTED;
													fail = 0xFF72;	  /* FEC_Sync miss */
												}
												else
												{
													scan_flag = RTV_CHANNEL_NOT_DETECTED;
													fail = 0xFF73;
												}
											}
											else
											{
												scan_flag = RTV_CHANNEL_NOT_DETECTED;
												fail = 0xFF74;
												goto TDMB_SCAN_EXIT;						 /* Auto-scan result return */
											}
										}
									}
									else
									{
										fail = 0xFF75;
										scan_flag = RTV_CHANNEL_NOT_DETECTED;	   /* OFDM_Lock & FEC_Miss */
									}
								}
								else
								{
									scan_flag = RTV_CHANNEL_NOT_DETECTED;	   /* OFDM_Unlock */
									fail = 0xFF0B;	
								}		
								goto TDMB_SCAN_EXIT;						 /* Auto-scan result return */
							}
							else
							{
								scan_flag = RTV_CHANNEL_NOT_DETECTED;	
							}
						}
						fail = 0xFF0C;	
						scan_flag = RTV_CHANNEL_NOT_DETECTED;
						goto TDMB_SCAN_EXIT;
					}
				}
			}
		}		
		else
		{
			fail = 0xFF0C;	
			scan_flag = RTV_CHANNEL_NOT_DETECTED;
			goto TDMB_SCAN_EXIT;
		}
	}

	fail = 0xFF0D;	
	
TDMB_SCAN_EXIT:

	RTV_GUARD_FREE;
	
	g_afRtvChannelChange[RaonTvChipIdx] = FALSE; 

	g_aTdmbPrevChFreqKHz[RaonTvChipIdx] = dwChFreqKHz;

	//RTV_DBGMSG1("[rtvTDMB_ScanFrequency] 0x%04X\n", fail);
	
	return scan_flag;	  /* Auto-scan result return */
}


void rtvTDMB_DisableStreamOut(void)
{
	RTV_GUARD_LOCK;
	
	rtv_StreamDisable(RTV_TV_MODE_TDMB);

	RTV_GUARD_FREE;
}


INT rtvTDMB_Initialize(E_RTV_COUNTRY_BAND_TYPE eRtvCountryBandType)
{
	INT nRet;

#ifdef RTV_DUAL_CHIP_USED 
	if((RaonTvChipIdx !=0) && (RaonTvChipIdx != 1))
	{
		RTV_DBGMSG0("[rtvTDMB_Initialize] Must selects a chip usinge Macro!\n");
		return RTV_INVALID_CHIP_IDX;
	}
	
	g_fRtvDiversityEnabled = FALSE;
#endif

	switch( eRtvCountryBandType )
	{
		case RTV_COUNTRY_BAND_KOREA:
			break;
			
		default:
			return RTV_INVAILD_COUNTRY_BAND;
	}
	g_eRtvCountryBandType = eRtvCountryBandType;

	g_aTdmbPrevChFreqKHz[RaonTvChipIdx] = 0;		
		
	g_aRtvUsedHwSubChIdxBits[RaonTvChipIdx] = 0x00;	

	g_afRtvTdmbConfiguredFIC[RaonTvChipIdx] = FALSE;	

	g_aRegSubChArrayIdxBits[RaonTvChipIdx] = 0x00;
	
	g_aRegSubChIdBits[RaonTvChipIdx][0] = 0x00000000;
	g_aRegSubChIdBits[RaonTvChipIdx][1] = 0x00000000;
	
	nRet = rtv_InitSystem(RTV_TV_MODE_TDMB, RTV_ADC_CLK_FREQ_8_MHz);
	if(nRet != RTV_SUCCESS)
		return nRet;

	/* Must after rtv_InitSystem() to save ADC clock. */
	tdmb_InitDemod();
	
	nRet = rtvRF_Initilize(RTV_TV_MODE_TDMB);
	if(nRet != RTV_SUCCESS)
		return nRet;

     RTV_DELAY_MS(100);
	  
	RTV_REG_MAP_SEL(RF_PAGE); 
	RTV_REG_SET( 0x6b,  0xC5);

#ifdef RTV_CIF_MODE_ENABLED /* CIF Mode */
	rtvCIFDEC_Init();	
#endif	

	return RTV_SUCCESS;
}

#endif /* #ifdef RTV_TDMB_ENABLE */





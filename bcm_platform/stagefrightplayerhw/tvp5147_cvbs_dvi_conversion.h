
#ifndef __TVP5147_EXTERNAL_CHIP__
#define __TVP5147_EXTERNAL_CHIP__

#define I2C_NUMBER 56
#define TVP5147_ADDRESS 0x5C

static int i2c_TVP5147_write_byte( NEXUS_I2cHandle hi2c, unsigned char offset,  unsigned char byteno,  unsigned char *p_data);
static int i2c_TVP5147_read_byte(NEXUS_I2cHandle hi2c,  unsigned char offset, unsigned char byteno, unsigned char *p_data);

static unsigned char TVP5147[I2C_NUMBER] =
{
	0x00,0x01,	/* CVBS VI_1B */
	0x02,0x00,	/* Auto-switch mode default */
	0x04,0x3f,	/* Gigabyte 3f */
	0x06,0x40,
	0x08,0x00,	/* Gigabyte 0 */
	0x0D,0x10,	/*TEST*/
	0x0E,0x04,	/* Gigabyte 4 */
	0x16,0x79,	/*AVID Start*/ /*0x79*/
	0x17,0x10,	/*AVID Start*/ /*Need*/
	0x18,0x47,	/*AVID Stop*/ /*0x47*/
	0x19,0x03,	/*AVID Stop*/ /*0x03*/
	0x1C,0x3E,	/*HS Stop*/
	0x1D,0x00,	/*HS Stop*/
	0x1E,0x05,	/*VS Start*/
	0x1F,0x00,	/*VS Start*/
	0x20,0x08,	/*VS Stop*/
	0x21,0x00,	/*VS Stop*/
	0x22,0x01,	/*VBLK Start*/
	0x23,0x00,	/*VBLK Start*/
	0x24,0x18,	/*VBLK Stop*/
	0x25,0x00,	/*VBLK Stop*/
	0x32,0x00,	/*0x03*/
	0x33,0x41,	/*extended range, 20-bit 422 with separate Syncs */
	0x34,0x11,	/*Clock and Data enable*/
	0x35,0xEA,	/*AVID out*/
	0x36,0xAF,	/* HS & VS active */
	0x39,0x01,
    0x75,0x1A,			/* Gigabyte 0x75,0x1A, */
};

static unsigned char rTVP5147[41] =
{
		0x00,
		0x01,	
		0x02,
		0x03,
		0x04,	
		0x05,
		0x06,
		0x08,
		0x0E,
		0x16,
		0x17,
		0x18,
		0x19,
		0x1A,
		0x1B,
		0x1C,
		0x1D,
		0x1E,
		0x1F,
		0x20,
		0x21,
		0x22,
		0x23,
		0x24,
		0x25,
		0x28,
		0x2F,
	0x31,
		0x32,
		0x33,
		0x34,
		0x35,
		0x36,	
	0x3A,
	0x3B,
	0x3F,
	0x40,
	0x41,
	0x42,
	0x43,
};

static unsigned char aucInputMux[36] = 
{
	62, 62,  8,  9,
	22, 23, 24, 25,
	26, 27, 28, 29,
	62, 62,  4,  5,
	12, 13, 14, 15,
	16, 17, 18, 19,
	62, 62, 62, 62,
	62, 62, 62, 62,
	62, 62, 62, 62
};

static int i2c_TVP5147_write_byte( NEXUS_I2cHandle hi2c, unsigned char offset,  unsigned char byteno,  unsigned char *p_data)
{
   	 unsigned char i;
	NEXUS_Error rc;

	if(hi2c == NULL)
	{
		BDBG_MSG(("NEXUS_I2cHandle == NULL"));
		return 1;
	}

	for(i = 0;i < 10;i++)
	{

		rc = NEXUS_I2c_Write(hi2c,TVP5147_ADDRESS,offset,p_data,byteno);
		if(rc == NEXUS_SUCCESS)
			break;
		else
			BKNI_Sleep(50);
	}
	
	if(rc != NEXUS_SUCCESS)
	{
		BDBG_ERR(("%15s@%5d:%25s,address = 0x%x,REG=0x%x \n",strrchr(__FILE__,'/')? strrchr(__FILE__,'/')+1 : __FILE__,__LINE__,__FUNCTION__,TVP5147_ADDRESS,offset));
		return 1;
	}
	else
	{
		return 0;
	}
}

static int i2c_TVP5147_read_byte(NEXUS_I2cHandle hi2c,  unsigned char offset, unsigned char byteno, unsigned char *p_data)
{
    int i;
	NEXUS_Error rc;

	if(hi2c == NULL)
	{
		BDBG_MSG(("NEXUS_I2cHandle == NULL"));
		return 1;
	}

	for(i = 0;i < 10;i++)
	{
 		rc = NEXUS_I2c_Read(hi2c,TVP5147_ADDRESS,offset,p_data,byteno);
		if(rc == NEXUS_SUCCESS)
			break;
		else
			BKNI_Sleep(50);
	}

	if(rc != NEXUS_SUCCESS)
	{
		BDBG_ERR(("%15s@%5d:%25s,address = 0x%x,REG=0x%x \n",strrchr(__FILE__,'/')? strrchr(__FILE__,'/')+1 : __FILE__,__LINE__,__FUNCTION__,TVP5147_ADDRESS,offset));
		return 1;
	}
	else
	{
		return 0;
	}
}

static void program_external_cvbs_dvi_chip(void)
{
    NEXUS_I2cHandle hi2c_Ch0;
    NEXUS_I2cSettings i2cSettings;
    int i;
    unsigned char cData,cData_2;
    unsigned char rgTVP5147[41];

    BDBG_ERR(("app aucMux 00000000000: %d %d %d %d !!!!!!!!!!!!!!!!",
    	aucInputMux[0], aucInputMux[1],
    	aucInputMux[2], aucInputMux[3]));

    NEXUS_I2c_GetDefaultSettings(&i2cSettings);
    /* default all other frontend i2c to be fast */
    i2cSettings.fourByteXferMode = true;
    i2cSettings.clockRate = NEXUS_I2cClockRate_e400Khz;

    hi2c_Ch0 = NEXUS_I2c_Open(0, &i2cSettings); 

    for(i = 0;i<I2C_NUMBER;i+=2)
    {
        cData = TVP5147[i+1];
        i2c_TVP5147_write_byte( hi2c_Ch0, TVP5147[i],  0x01,  &cData);
        BKNI_Sleep(50);
    }

    for(i = 0;i<40;i++)
    {
        cData = rTVP5147[i];
        rgTVP5147[i] = rTVP5147[i];
        i2c_TVP5147_read_byte( hi2c_Ch0, rTVP5147[i], 0x01,  &cData_2);
        rgTVP5147[i+1] = cData_2;
        BDBG_ERR(("read 0x%x,0x%x\n",rgTVP5147[i],rgTVP5147[i+1]));
    }
    BDBG_ERR(("I2C read done \n"));
    
    NEXUS_I2c_Close(hi2c_Ch0);

}

#endif /* #define __TVP5147_EXTERNAL_CHIP__ */

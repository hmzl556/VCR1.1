#include "rtc.h"
#include "rcc.h"


/* Masks Definition */
#define RTC_TR_RESERVED_MASK   ((uint32_t)0x007F7F7F)
#define RTC_DATE_RESERVED_MASK ((uint32_t)0x00FFFF3F)
#define RTC_INITM_MASK          ((uint32_t)0xFFFFFF7F)  
#define RTC_RSF_MASK           ((uint32_t)0xFFFFFFDF)
#define RTC_FLAGS_MASK    ((uint32_t)(RTC_FLAG_WTF | RTC_FLAG_ALAF | RTC_FLAG_INITF | RTC_FLAG_RSYF \
| RTC_FLAG_INITSF | RTC_FLAG_WTWF | RTC_FLAG_ALAWF | RTC_FLAG_RECPF | RTC_FLAG_SHOPF))

#define INITMODE_TIMEOUT ((uint32_t)0x00002000)
#define SYNCHRO_TIMEOUT  ((uint32_t)0x00008000)
#define RECALPF_TIMEOUT  ((uint32_t)0x00001000)
#define SHPF_TIMEOUT     ((uint32_t)0x00002000)

static uint8_t RTC_ByteToBcd2(uint8_t Value);
static uint8_t RTC_Bcd2ToByte(uint8_t Value);

/*-------------------------------------------------------------------
ErrorStatus RTC_DeInit(void)RTC设备初始化
返回	SUCCESS: 	RTC registers are deinitialized
			ERROR: 		RTC registers are not deinitialized
-------------------------------------------------------------------*/
ErrorStatus RTC_DeInit(void)
{
    __IO uint32_t wutcounter = 0x00;
    uint32_t wutwfstatus     = 0x00;
    ErrorStatus status       = ERROR;

    /* Disable the write protection for RTC registers */
    RTC_EnableWriteProtection(DISABLE);

    /* Set Initialization mode */
    if (RTC_EnterInitMode() == ERROR)
    {
        status = ERROR;
    }
    else
    {
        /* Reset TSH, DAT and CTRL registers */
        RTC->TSH  = (uint32_t)0x00000000;
        RTC->DATE = (uint32_t)0x00002101;

        /* Reset All CTRL bits except CTRL[2:0] and CTRL[7] lock bit*/
        RTC->CTRL &= (uint32_t)0x00000087;

        /* Wait till RTC WTWF flag is set and if Time out is reached exit */
        do
        {
            wutwfstatus = RTC->INITSTS & RTC_INITSTS_WTWF;
            wutcounter++;
        } while ((wutcounter != INITMODE_TIMEOUT) && (wutwfstatus == 0x00));

        if ((RTC->INITSTS & RTC_INITSTS_WTWF) == RESET)
        {
            status = ERROR;
        }
        else
        {
            /* Reset all RTC CTRL register bits */
            RTC->CTRL &= (uint32_t)0x00000080; /* Bit CTRL[7] is a exception*/
            RTC->WKUPT   = (uint32_t)0x0000FFFF;
            RTC->PRE     = (uint32_t)0x007F00FF;
            RTC->ALARMA  = (uint32_t)0x00000000;
            RTC->SCTRL   = (uint32_t)0x00000000;
            RTC->CALIB   = (uint32_t)0x00000000;
            RTC->ALRMASS = (uint32_t)0x00000000;

            /* Reset INITSTS register and exit initialization mode */
            RTC->INITSTS &= (uint32_t)0x000000FF;
            /* Exit Initialization mode */
            RTC_ExitInitMode();

            /* Wait till the RTC RSYF flag is set */
            if (RTC_WaitForSynchro() == ERROR)
            {
                status = ERROR;
            }
            else
            {
                status = SUCCESS;
            }
        }
    }

    /* Enable the write protection for RTC registers */
    RTC_EnableWriteProtection(ENABLE);

    return status;
}

/*-------------------------------------------------------------------
ErrorStatus RTC_Init(RTC_InitType* RTC_InitStruct)RTC初始化
返回	SUCCESS: 	RTC registers are initialized
			ERROR: 		RTC registers are not initialized
-------------------------------------------------------------------*/
ErrorStatus RTC_Init(RTC_InitType* RTC_InitStruct)
{
    ErrorStatus status = ERROR;

    /* Disable the write protection for RTC registers */
    RTC_EnableWriteProtection(DISABLE);
    
    /* Set Initialization mode */
    if (RTC_EnterInitMode() == ERROR)
    {
        status = ERROR;
    }
    else
    {
        /* Clear RTC CTRL HFMT Bit */
        RTC->CTRL &= ((uint32_t) ~(RTC_CTRL_HFMT));
        /* Set RTC_CTRL register */
        RTC->CTRL |= ((uint32_t)(RTC_InitStruct->RTC_HourFormat));

        /* Configure the RTC PRE */
        RTC->PRE = (uint32_t)(RTC_InitStruct->RTC_SynchPrediv);
        RTC->PRE |= (uint32_t)(RTC_InitStruct->RTC_AsynchPrediv << 16);

        /* Exit Initialization mode */
        RTC_ExitInitMode();

        status = SUCCESS;
    }
    /* Enable the write protection for RTC registers */
    RTC_EnableWriteProtection(ENABLE);

    return status;
}

/*-------------------------------------------------------------------
void RTC_StructInit(RTC_InitType* RTC_InitStruct)初始化RTC初始化所需要
			的数据结构
-------------------------------------------------------------------*/
void RTC_StructInit(RTC_InitType* RTC_InitStruct)
{
    /* Initialize the RTC_HourFormat member */
    RTC_InitStruct->RTC_HourFormat = RTC_24HOUR_FORMAT;

    /* Initialize the RTC_AsynchPrediv member */
    RTC_InitStruct->RTC_AsynchPrediv = (uint32_t)0x7F;

    /* Initialize the RTC_SynchPrediv member */
    RTC_InitStruct->RTC_SynchPrediv = (uint32_t)0xFF;
}

/*-------------------------------------------------------------------
void RTC_EnableWriteProtection(FunctionalState Cmd)许可/禁止RTC的写入
		保护 ENABLE or DISABLE.
-------------------------------------------------------------------*/
void RTC_EnableWriteProtection(FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Enable the write protection for RTC registers */
        RTC->CTRL&= 0xFFFFFF7F;
    }
    else
    {
        /* Disable the write protection for RTC registers */
        RTC->WRP = 0xCA;
        RTC->WRP = 0x53;
        RTC->CTRL|= 0x00000080;
    }
}

/*-------------------------------------------------------------------
ErrorStatus RTC_EnterInitMode(void)	RTC进入初始化模式
返回	SUCCESS: 	RTC is in Init mode
			ERROR: 		RTC is not in Init mode
-------------------------------------------------------------------*/
ErrorStatus RTC_EnterInitMode(void)
{
    __IO uint32_t initcounter = 0x00;
    ErrorStatus status        = ERROR;
    uint32_t initstatus       = 0x00;

    /* Check if the Initialization mode is set */
    if ((RTC->INITSTS & RTC_INITSTS_INITF) == (uint32_t)RESET)
    {
        /* Set the Initialization mode */
        RTC->INITSTS |= (uint32_t)(~RTC_INITM_MASK) ;

        /* Wait till RTC is in INIT state and if Time out is reached exit */
        do
        {
            initstatus = RTC->INITSTS & RTC_INITSTS_INITF;
            initcounter++;
        } while ((initcounter != INITMODE_TIMEOUT) && (initstatus == 0x00));

        if ((RTC->INITSTS & RTC_INITSTS_INITF) != RESET)
        {
            status = SUCCESS;
        }
        else
        {
            status = ERROR;
        }
    }
    else
    {
        status = SUCCESS;
    }

    return (status);
}

/*-------------------------------------------------------------------
void RTC_ExitInitMode(void)RTC推出初始化模式,推出初始化模式后4个RTCCLK
			后开始RTC的寄存器计数
-------------------------------------------------------------------*/
void RTC_ExitInitMode(void)
{
    /* Exit Initialization mode */
    RTC->INITSTS &= (uint32_t)~RTC_INITSTS_INITM;
}

/*-------------------------------------------------------------------
ErrorStatus RTC_WaitForSynchro(void)等待RTC的数据寄存器由RTC APB时钟
						同步完成
返回	SUCCESS: 	RTC registers are synchronised
			ERROR: 		RTC registers are not synchronised
-------------------------------------------------------------------*/
ErrorStatus RTC_WaitForSynchro(void)
{
    __IO uint32_t synchrocounter = 0;
    ErrorStatus status           = ERROR;
    uint32_t synchrostatus       = 0x00;

    /* Disable the write protection for RTC registers */
    RTC_EnableWriteProtection(DISABLE);

    /* Clear RSYF flag */
    RTC->INITSTS &= (uint32_t)RTC_RSF_MASK;

    /* Wait the registers to be synchronised */
    do
    {
        synchrostatus = RTC->INITSTS & RTC_INITSTS_RSYF;
        synchrocounter++;
    } while ((synchrocounter != SYNCHRO_TIMEOUT) && (synchrostatus == 0x00));

    if ((RTC->INITSTS & RTC_INITSTS_RSYF) != RESET)
    {
        status = SUCCESS;
    }
    else
    {
        status = ERROR;
    }

    /* Enable the write protection for RTC registers */
    RTC_EnableWriteProtection(ENABLE);

    return (status);
}

/*-------------------------------------------------------------------
void RTC_EnableBypassShadow(FunctionalState Cmd)允许/禁止RTC的旁路影子
			寄存器		ENABLE or DISABLE.
-------------------------------------------------------------------*/
void RTC_EnableBypassShadow(FunctionalState Cmd)
{

    /* Disable the write protection for RTC registers */
    RTC_EnableWriteProtection(DISABLE);

    if (Cmd != DISABLE)
    {
        /* Set the BYPS bit */
        RTC->CTRL |= (uint8_t)RTC_CTRL_BYPS;
    }
    else
    {
        /* Reset the BYPS bit */
        RTC->CTRL &= (uint8_t)~RTC_CTRL_BYPS;
    }

    /* Enable the write protection for RTC registers */
    RTC_EnableWriteProtection(ENABLE);
}

/*-------------------------------------------------------------------
ErrorStatus RTC_ConfigTime(uint32_t RTC_Format,RTC_TimeType* RTC_TimeStruct)
			设置RTC的当前时间
			RTC_Format = 	RTC_FORMAT_BIN 	Binary data format.
										RTC_FORMAT_BCD 	BCD data format.
			RTC_TimeStruct = 时间结构
返回	SUCCESS:	RTC Time register is configured
			ERROR: 		RTC Time register is not configured
-------------------------------------------------------------------*/
ErrorStatus RTC_ConfigTime(uint32_t RTC_Format, RTC_TimeType* RTC_TimeStruct)
{
    uint32_t tmpregister = 0;
    ErrorStatus status   = ERROR;

    if (RTC_Format == RTC_FORMAT_BIN)
    {
        if ((RTC->CTRL & RTC_CTRL_HFMT) == (uint32_t)RESET)
        {
            RTC_TimeStruct->H12 = 0x00;
        }
    }
    else
    {
        if ((RTC->CTRL & RTC_CTRL_HFMT) != (uint32_t)RESET)
        {
            tmpregister = RTC_Bcd2ToByte(RTC_TimeStruct->Hours);
        }
        else
        {
            RTC_TimeStruct->H12 = 0x00;
        }
    }

    /* Check the input parameters format */
    if (RTC_Format != RTC_FORMAT_BIN)
    {
        tmpregister = (((uint32_t)(RTC_TimeStruct->Hours) << 16) | ((uint32_t)(RTC_TimeStruct->Minutes) << 8)
                       | ((uint32_t)RTC_TimeStruct->Seconds) | ((uint32_t)(RTC_TimeStruct->H12) << 16));
    }
    else
    {
        tmpregister =
            (uint32_t)(((uint32_t)RTC_ByteToBcd2(RTC_TimeStruct->Hours) << 16)
                       | ((uint32_t)RTC_ByteToBcd2(RTC_TimeStruct->Minutes) << 8)
                       | ((uint32_t)RTC_ByteToBcd2(RTC_TimeStruct->Seconds)) | (((uint32_t)RTC_TimeStruct->H12) << 16));
    }

    /* Disable the write protection for RTC registers */
    RTC_EnableWriteProtection(DISABLE);

    /* Set Initialization mode */
    if (RTC_EnterInitMode() == ERROR)
    {
        status = ERROR;
    }
    else
    {
        /* Set the RTC_TSH register */
        RTC->TSH = (uint32_t)(tmpregister & RTC_TR_RESERVED_MASK);

        /* Exit Initialization mode */
        RTC_ExitInitMode();

        /* If  RTC_CTRL_BYPS bit = 0, wait for synchro else this check is not needed */
        if ((RTC->CTRL & RTC_CTRL_BYPS) == RESET)
        {
            if (RTC_WaitForSynchro() == ERROR)
            {
                status = ERROR;
            }
            else
            {
                status = SUCCESS;
            }
        }
        else
        {
            status = SUCCESS;
        }
    }
    /* Enable the write protection for RTC registers */
    RTC_EnableWriteProtection(ENABLE);

    return status;
}

/*-------------------------------------------------------------------
void RTC_TimeStructInit(RTC_TimeType* RTC_TimeStruct)
		初始化RTC时间初始化的数据结构
-------------------------------------------------------------------*/
void RTC_TimeStructInit(RTC_TimeType* RTC_TimeStruct)
{
    /* Time = 00h:00min:00sec */
    RTC_TimeStruct->H12     = RTC_AM_H12;
    RTC_TimeStruct->Hours   = 0;
    RTC_TimeStruct->Minutes = 0;
    RTC_TimeStruct->Seconds = 0;
}

/*-------------------------------------------------------------------
void RTC_GetTime(uint32_t RTC_Format, RTC_TimeType* RTC_TimeStruct)
		读取RTC的当前时间	
		RTC_Format = 	RTC_FORMAT_BIN Binary data format.
									RTC_FORMAT_BCD BCD data format.
		返回数据放在 RTC_TimeStruct内部
-------------------------------------------------------------------*/
void RTC_GetTime(uint32_t RTC_Format, RTC_TimeType* RTC_TimeStruct)
{
    uint32_t tmpregister = 0;


    /* Get the RTC_TSH register */
    tmpregister = (uint32_t)(RTC->TSH & RTC_TR_RESERVED_MASK);

    /* Fill the structure fields with the read parameters */
    RTC_TimeStruct->Hours   = (uint8_t)((tmpregister & (RTC_TSH_HOT | RTC_TSH_HOU)) >> 16);
    RTC_TimeStruct->Minutes = (uint8_t)((tmpregister & (RTC_TSH_MIT | RTC_TSH_MIU)) >> 8);
    RTC_TimeStruct->Seconds = (uint8_t)(tmpregister & (RTC_TSH_SCT | RTC_TSH_SCU));
    RTC_TimeStruct->H12     = (uint8_t)((tmpregister & (RTC_TSH_APM)) >> 16);

    /* Check the input parameters format */
    if (RTC_Format == RTC_FORMAT_BIN)
    {
        /* Convert the structure parameters to Binary format */
        RTC_TimeStruct->Hours   = (uint8_t)RTC_Bcd2ToByte(RTC_TimeStruct->Hours);
        RTC_TimeStruct->Minutes = (uint8_t)RTC_Bcd2ToByte(RTC_TimeStruct->Minutes);
        RTC_TimeStruct->Seconds = (uint8_t)RTC_Bcd2ToByte(RTC_TimeStruct->Seconds);
    }
}

/*-------------------------------------------------------------------
uint32_t RTC_GetSubSecond(void)读取RTC的 秒内数据
-------------------------------------------------------------------*/
uint32_t RTC_GetSubSecond(void)
{
    uint32_t tmpregister = 0;

    /* Get subseconds values from the correspondent registers*/
    tmpregister = (uint32_t)(RTC->SUBS);

    /* Read DAT register to unfroze calendar registers */
    (void)(RTC->DATE);

    return (tmpregister);
}

/*-------------------------------------------------------------------
ErrorStatus RTC_SetDate(uint32_t RTC_Format, RTC_DateType* RTC_DateStruct)
		设置RTC的日期	
		RTC_Format = 	RTC_FORMAT_BIN Binary data format.
									RTC_FORMAT_BCD BCD data format.
		RTC_DateStruct为date的数据结构
返回	SUCCESS:	RTC Date register is configured
			ERROR: 		RTC Date register is not configured
-------------------------------------------------------------------*/
ErrorStatus RTC_SetDate(uint32_t RTC_Format, RTC_DateType* RTC_DateStruct)
{
    uint32_t tmpregister = 0;
    ErrorStatus status   = ERROR;

    if ((RTC_Format == RTC_FORMAT_BIN) && ((RTC_DateStruct->Month & 0x10) == 0x10))
    {
        RTC_DateStruct->Month = (RTC_DateStruct->Month & (uint32_t) ~(0x10)) + 0x0A;
    }
    if (RTC_Format == RTC_FORMAT_BCD)
    {
        tmpregister = RTC_Bcd2ToByte(RTC_DateStruct->Month);
        tmpregister = RTC_Bcd2ToByte(RTC_DateStruct->Date);
    }

    /* Check the input parameters format */
    if (RTC_Format != RTC_FORMAT_BIN)
    {
        tmpregister = ((((uint32_t)RTC_DateStruct->Year) << 16) | (((uint32_t)RTC_DateStruct->Month) << 8)
                       | ((uint32_t)RTC_DateStruct->Date) | (((uint32_t)RTC_DateStruct->WeekDay) << 13));
    }
    else
    {
        tmpregister = (((uint32_t)RTC_ByteToBcd2(RTC_DateStruct->Year) << 16)
                       | ((uint32_t)RTC_ByteToBcd2(RTC_DateStruct->Month) << 8)
                       | ((uint32_t)RTC_ByteToBcd2(RTC_DateStruct->Date)) | ((uint32_t)RTC_DateStruct->WeekDay << 13));
    }

    /* Disable the write protection for RTC registers */
    RTC_EnableWriteProtection(DISABLE);

    /* Set Initialization mode */
    if (RTC_EnterInitMode() == ERROR)
    {
        status = ERROR;
    }
    else
    {
        /* Set the RTC_DATE register */
        RTC->DATE = (uint32_t)(tmpregister & RTC_DATE_RESERVED_MASK);

        /* Exit Initialization mode */
        RTC_ExitInitMode();

        /* If  RTC_CTRL_BYPS bit = 0, wait for synchro else this check is not needed */
        if ((RTC->CTRL & RTC_CTRL_BYPS) == RESET)
        {
            if (RTC_WaitForSynchro() == ERROR)
            {
                status = ERROR;
            }
            else
            {
                status = SUCCESS;
            }
        }
        else
        {
            status = SUCCESS;
        }
    }
    /* Enable the write protection for RTC registers */
    RTC_EnableWriteProtection(ENABLE);

    return status;
}

/*-------------------------------------------------------------------
void RTC_DateStructInit(RTC_DateType* RTC_DateStruct)
			初始化DATE数据结构
-------------------------------------------------------------------*/
void RTC_DateStructInit(RTC_DateType* RTC_DateStruct)
{
    /* Monday, January 01 xx00 */
    RTC_DateStruct->WeekDay = RTC_WEEKDAY_MONDAY;
    RTC_DateStruct->Date    = 1;
    RTC_DateStruct->Month   = RTC_MONTH_JANUARY;
    RTC_DateStruct->Year    = 0;
}

/*-------------------------------------------------------------------
void RTC_GetDate(uint32_t RTC_Format, RTC_DateType* RTC_DateStruct)
		读取RTC的当前日期
		RTC_Format = 	RTC_FORMAT_BIN Binary data format.
									RTC_FORMAT_BCD BCD data format.
		返回的数据放在 RTC_DateStruct内部
-------------------------------------------------------------------*/
void RTC_GetDate(uint32_t RTC_Format, RTC_DateType* RTC_DateStruct)
{
    uint32_t tmpregister = 0;


    /* Get the RTC_TSH register */
    tmpregister = (uint32_t)(RTC->DATE & RTC_DATE_RESERVED_MASK);

    /* Fill the structure fields with the read parameters */
    RTC_DateStruct->Year    = (uint8_t)((tmpregister & (RTC_DATE_YRT | RTC_DATE_YRU)) >> 16);
    RTC_DateStruct->Month   = (uint8_t)((tmpregister & (RTC_DATE_MOT | RTC_DATE_MOU)) >> 8);
    RTC_DateStruct->Date    = (uint8_t)(tmpregister & (RTC_DATE_DAT | RTC_DATE_DAU));
    RTC_DateStruct->WeekDay = (uint8_t)((tmpregister & (RTC_DATE_WDU)) >> 13);

    /* Check the input parameters format */
    if (RTC_Format == RTC_FORMAT_BIN)
    {
        /* Convert the structure parameters to Binary format */
        RTC_DateStruct->Year  = (uint8_t)RTC_Bcd2ToByte(RTC_DateStruct->Year);
        RTC_DateStruct->Month = (uint8_t)RTC_Bcd2ToByte(RTC_DateStruct->Month);
        RTC_DateStruct->Date  = (uint8_t)RTC_Bcd2ToByte(RTC_DateStruct->Date);
    }
}

/*-------------------------------------------------------------------
void RTC_SetAlarm(uint32_t RTC_Format,uint32_t RTC_Alarm,RTC_AlarmType* RTC_AlarmStruct)
		设置闹钟
		RTC_Format = 	RTC_FORMAT_BIN 	Binary data format.
									RTC_FORMAT_BCD 	BCD data format.
		RTC_Alarm = 	RTC_A_ALARM 		to select Alarm A.
		RTC_AlarmStruct 为闹钟数据结构
-------------------------------------------------------------------*/
void RTC_SetAlarm(uint32_t RTC_Format, uint32_t RTC_Alarm, RTC_AlarmType* RTC_AlarmStruct)
{
    uint32_t tmpregister = 0;

    if (RTC_Format == RTC_FORMAT_BIN)
    {
        if ((RTC->CTRL & RTC_CTRL_HFMT) == (uint32_t)RESET)
        {
            RTC_AlarmStruct->AlarmTime.H12 = 0x00;
        }
    }
    else
    {
        if ((RTC->CTRL & RTC_CTRL_HFMT) != (uint32_t)RESET)
        {
            tmpregister = RTC_Bcd2ToByte(RTC_AlarmStruct->AlarmTime.Hours);
        }
        else
        {
            RTC_AlarmStruct->AlarmTime.H12 = 0x00;
        }


        if (RTC_AlarmStruct->DateWeekMode == RTC_ALARM_SEL_WEEKDAY_DATE)
        {
            tmpregister = RTC_Bcd2ToByte(RTC_AlarmStruct->DateWeekValue);
        }
        else
        {
            tmpregister = RTC_Bcd2ToByte(RTC_AlarmStruct->DateWeekValue);
        }
    }

    /* Check the input parameters format */
    if (RTC_Format != RTC_FORMAT_BIN)
    {
        tmpregister =
            (((uint32_t)(RTC_AlarmStruct->AlarmTime.Hours) << 16)
             | ((uint32_t)(RTC_AlarmStruct->AlarmTime.Minutes) << 8) | ((uint32_t)RTC_AlarmStruct->AlarmTime.Seconds)
             | ((uint32_t)(RTC_AlarmStruct->AlarmTime.H12) << 16) | ((uint32_t)(RTC_AlarmStruct->DateWeekValue) << 24)
             | ((uint32_t)RTC_AlarmStruct->DateWeekMode) | ((uint32_t)RTC_AlarmStruct->AlarmMask));
    }
    else
    {
        tmpregister = (((uint32_t)RTC_ByteToBcd2(RTC_AlarmStruct->AlarmTime.Hours) << 16)
                       | ((uint32_t)RTC_ByteToBcd2(RTC_AlarmStruct->AlarmTime.Minutes) << 8)
                       | ((uint32_t)RTC_ByteToBcd2(RTC_AlarmStruct->AlarmTime.Seconds))
                       | ((uint32_t)(RTC_AlarmStruct->AlarmTime.H12) << 16)
                       | ((uint32_t)RTC_ByteToBcd2(RTC_AlarmStruct->DateWeekValue) << 24)
                       | ((uint32_t)RTC_AlarmStruct->DateWeekMode) | ((uint32_t)RTC_AlarmStruct->AlarmMask));
    }

    /* Disable the write protection for RTC registers */
    RTC_EnableWriteProtection(DISABLE);

    /* Configure the Alarm register */
    if (RTC_Alarm == RTC_A_ALARM)
    {
        RTC->ALARMA = (uint32_t)tmpregister;
    }

    /* Enable the write protection for RTC registers */
    RTC_EnableWriteProtection(ENABLE);
}

/*-------------------------------------------------------------------
void RTC_AlarmStructInit(RTC_AlarmType* RTC_AlarmStruct)
		闹钟数据结构初始化
-------------------------------------------------------------------*/
void RTC_AlarmStructInit(RTC_AlarmType* RTC_AlarmStruct)
{
    /* Alarm Time Settings : Time = 00h:00mn:00sec */
    RTC_AlarmStruct->AlarmTime.H12     = RTC_AM_H12;
    RTC_AlarmStruct->AlarmTime.Hours   = 0;
    RTC_AlarmStruct->AlarmTime.Minutes = 0;
    RTC_AlarmStruct->AlarmTime.Seconds = 0;

    /* Alarm Date Settings : Date = 1st day of the month */
    RTC_AlarmStruct->DateWeekMode  = RTC_ALARM_SEL_WEEKDAY_DATE;
    RTC_AlarmStruct->DateWeekValue = 1;

    /* Alarm Masks Settings : Mask =  all fields are not masked */
    RTC_AlarmStruct->AlarmMask = RTC_ALARMMASK_NONE;
}

/*-------------------------------------------------------------------
void RTC_GetAlarm(uint32_t RTC_Format,uint32_t RTC_Alarm,RTC_AlarmType* RTC_AlarmStruct)
			读取RTC的闹钟信息
			RTC_Format = 	RTC_FORMAT_BIN 	Binary data format.
										RTC_FORMAT_BCD 	BCD data format.
			RTC_Alarm = RTC_A_ALARM 			to select Alarm A.
			读取的数据填充到 RTC_AlarmStruct
-------------------------------------------------------------------*/
void RTC_GetAlarm(uint32_t RTC_Format, uint32_t RTC_Alarm, RTC_AlarmType* RTC_AlarmStruct)
{
    uint32_t tmpregister = 0;

    /* Get the RTC_ALARMx register */
    if (RTC_Alarm == RTC_A_ALARM)
    {
        tmpregister = (uint32_t)(RTC->ALARMA);
    }

    /* Fill the structure with the read parameters */
    RTC_AlarmStruct->AlarmTime.Hours   = (uint32_t)((tmpregister & (RTC_ALARMA_HOT | RTC_ALARMA_HOU)) >> 16);
    RTC_AlarmStruct->AlarmTime.Minutes = (uint32_t)((tmpregister & (RTC_ALARMA_MIT | RTC_ALARMA_MIU)) >> 8);
    RTC_AlarmStruct->AlarmTime.Seconds = (uint32_t)(tmpregister & (RTC_ALARMA_SET | RTC_ALARMA_SEU));
    RTC_AlarmStruct->AlarmTime.H12     = (uint32_t)((tmpregister & RTC_ALARMA_APM) >> 16);
    RTC_AlarmStruct->DateWeekValue     = (uint32_t)((tmpregister & (RTC_ALARMA_DTT | RTC_ALARMA_DTU)) >> 24);
    RTC_AlarmStruct->DateWeekMode      = (uint32_t)(tmpregister & RTC_ALARMA_WKDSEL);
    RTC_AlarmStruct->AlarmMask         = (uint32_t)(tmpregister & RTC_ALARMMASK_ALL);

    if (RTC_Format == RTC_FORMAT_BIN)
    {
        RTC_AlarmStruct->AlarmTime.Hours   = RTC_Bcd2ToByte(RTC_AlarmStruct->AlarmTime.Hours);
        RTC_AlarmStruct->AlarmTime.Minutes = RTC_Bcd2ToByte(RTC_AlarmStruct->AlarmTime.Minutes);
        RTC_AlarmStruct->AlarmTime.Seconds = RTC_Bcd2ToByte(RTC_AlarmStruct->AlarmTime.Seconds);
        RTC_AlarmStruct->DateWeekValue     = RTC_Bcd2ToByte(RTC_AlarmStruct->DateWeekValue);
    }
}

/*-------------------------------------------------------------------
ErrorStatus RTC_EnableAlarm(uint32_t RTC_Alarm, FunctionalState Cmd)
		允许/禁止 Rtc的闹钟
		RTC_Alarm = RTC_A_ALARM to select Alarm A.
		Cmd = ENABLE or DISABLE.
		返回 = 	SUCCESS: 	RTC Alarm is enabled/disabled
						ERROR: 		RTC Alarm is not enabled/disabled
-------------------------------------------------------------------*/
ErrorStatus RTC_EnableAlarm(uint32_t RTC_Alarm, FunctionalState Cmd)
{
    __IO uint32_t alarmcounter = 0x00;
    uint32_t alarmstatus       = 0x00;
    ErrorStatus status         = ERROR;

    /* Disable the write protection for RTC registers */
    RTC_EnableWriteProtection(DISABLE);

    /* Configure the Alarm state */
    if (Cmd != DISABLE)
    {
        RTC->CTRL |= (uint32_t)RTC_Alarm;

        status = SUCCESS;
    }
    else
    {
        /* Disable the Alarm in RTC_CTRL register */
        RTC->CTRL &= (uint32_t)~RTC_Alarm;

        /* Wait till RTC ALxWF flag is set and if Time out is reached exit */
        do
        {
            alarmstatus = RTC->INITSTS & (RTC_Alarm >> 8);
            alarmcounter++;
        } while ((alarmcounter != INITMODE_TIMEOUT) && (alarmstatus == 0x00));

        if ((RTC->INITSTS & (RTC_Alarm >> 8)) == RESET)
        {
            status = ERROR;
        }
        else
        {
            status = SUCCESS;
        }
    }

    /* Enable the write protection for RTC registers */
    RTC_EnableWriteProtection(ENABLE);

    return status;
}

/*-------------------------------------------------------------------
void RTC_ConfigAlarmSubSecond(uint32_t RTC_Alarm,uint32_t RTC_AlarmSubSecondValue,uint32_t RTC_AlarmSubSecondMask)
			初始化 RTC AlarmA模式下秒内数据和掩码
			RTC_Alarm = RTC_A_ALARM 	to select Alarm A.
			RTC_AlarmSubSecondValue = 0 - 0x00007ffff
			RTC_AlarmSubSecondMask 为秒内数据掩码：
			RTC_SUBS_MASK_ALL 			All Alarm SS fields are masked.
			RTC_SUBS_MASK_SS14_1 		SS[14:1] are don't care in Alarm comparison.
															Only SS[0] is compared
			RTC_SUBS_MASK_SS14_2 		SS[14:2] are don't care in Alarm comparison.
															Only SS[1:0] are compared
			RTC_SUBS_MASK_SS14_3 		SS[14:3] are don't care in Alarm comparison.
															Only SS[2:0] are compared
			RTC_SUBS_MASK_SS14_4 		SS[14:4] are don't care in Alarm comparison.
															Only SS[3:0] are compared
			RTC_SUBS_MASK_SS14_5 		SS[14:5] are don't care in Alarm comparison.
															Only SS[4:0] are compared.
			RTC_SUBS_MASK_SS14_6 		SS[14:6] are don't care in Alarm comparison.
															Only SS[5:0] are compared.
			RTC_SUBS_MASK_SS14_7 		SS[14:7] are don't care in Alarm comparison.
															Only SS[6:0] are compared.
			RTC_SUBS_MASK_SS14_8 		SS[14:8] are don't care in Alarm comparison.
															Only SS[7:0] are compared.
			RTC_SUBS_MASK_SS14_9 		SS[14:9] are don't care in Alarm comparison.
															Only SS[8:0] are compared.
			RTC_SUBS_MASK_SS14_10 	SS[14:10] are don't care in Alarm comparison.
															Only SS[9:0] are compared.
			RTC_SUBS_MASK_SS14_11 	SS[14:11] are don't care in Alarm comparison.
															Only SS[10:0] are compared.
			RTC_SUBS_MASK_SS14_12 	SS[14:12] are don't care in Alarm comparison.
															Only SS[11:0] are compared.
			RTC_SUBS_MASK_SS14_13 	SS[14:13] are don't care in Alarm comparison.
															Only SS[12:0] are compared.
			RTC_SUBS_MASK_SS14_14 	SS[14] is don't care in Alarm comparison.
															Only SS[13:0] are compared.
			RTC_SUBS_MASK_NONE 			SS[14:0] are compared and must match
															to activate alarm.
-------------------------------------------------------------------*/
void RTC_ConfigAlarmSubSecond(uint32_t RTC_Alarm, uint32_t RTC_AlarmSubSecondValue, uint32_t RTC_AlarmSubSecondMask)
{
    uint32_t tmpregister = 0;


    /* Disable the write protection for RTC registers */
    RTC_EnableWriteProtection(DISABLE);

    /* Configure the Alarm A or Alarm B SubSecond registers */
    tmpregister = (uint32_t)(uint32_t)(RTC_AlarmSubSecondValue) | (uint32_t)(RTC_AlarmSubSecondMask);

    if (RTC_Alarm == RTC_A_ALARM)
    {
        /* Configure the AlarmA SubSecond register */
        RTC->ALRMASS = tmpregister;
    }

    /* Enable the write protection for RTC registers */
    RTC_EnableWriteProtection(ENABLE);
}

/*-------------------------------------------------------------------
uint32_t RTC_GetAlarmSubSecond(uint32_t RTC_Alarm)读取RTC闹钟的秒内
				闹钟值
-------------------------------------------------------------------*/
uint32_t RTC_GetAlarmSubSecond(uint32_t RTC_Alarm)
{
    uint32_t tmpregister = 0;

    /* Get the RTC_ALARMx register */
    if (RTC_Alarm == RTC_A_ALARM)
    {
        tmpregister = (uint32_t)((RTC->ALRMASS) & RTC_ALRMASS_SSV);
    }

    return (tmpregister);
}

/*-------------------------------------------------------------------
void RTC_ConfigWakeUpClock(uint32_t RTC_WakeUpClock)初始化RTC唤醒时钟
		源
RTC_WakeUpClock = RTC_WKUPCLK_RTCCLK_DIV16 	RTC Wakeup Counter Clock = RTCCLK/16.
									RTC_WKUPCLK_RTCCLK_DIV8 	RTC Wakeup Counter Clock = RTCCLK/8.
									RTC_WKUPCLK_RTCCLK_DIV4 	RTC Wakeup Counter Clock = RTCCLK/4.
									RTC_WKUPCLK_RTCCLK_DIV2 	RTC Wakeup Counter Clock = RTCCLK/2.
									RTC_WKUPCLK_CK_SPRE_16BITS RTC Wakeup Counter Clock = CK_SPRE.
									RTC_WKUPCLK_CK_SPRE_17BITS RTC Wakeup Counter Clock = CK_SPRE.
-------------------------------------------------------------------*/
void RTC_ConfigWakeUpClock(uint32_t RTC_WakeUpClock)
{
    /* Disable the write protection for RTC registers */
    RTC_EnableWriteProtection(DISABLE);

    /* Clear the Wakeup Timer clock source bits in CTRL register */
    RTC->CTRL &= (uint32_t)~RTC_CTRL_WKUPSEL;

    /* Configure the clock source */
    RTC->CTRL |= (uint32_t)RTC_WakeUpClock;

    /* Enable the write protection for RTC registers */
    RTC_EnableWriteProtection(ENABLE);
}

/*-------------------------------------------------------------------
void RTC_SetWakeUpCounter(uint32_t RTC_WakeUpCounter)
			设置RTC的唤醒计数
			RTC_WakeUpCounter = 唤醒计数值 0x0000 to 0xFFFF.
-------------------------------------------------------------------*/
void RTC_SetWakeUpCounter(uint32_t RTC_WakeUpCounter)
{
    /* Disable the write protection for RTC registers */
    RTC_EnableWriteProtection(DISABLE);

    /* Configure the Wakeup Timer counter */
    RTC->WKUPT = (uint32_t)RTC_WakeUpCounter;

    /* Enable the write protection for RTC registers */
    RTC_EnableWriteProtection(ENABLE);
}

/*-------------------------------------------------------------------
uint32_t RTC_GetWakeUpCounter(void)读取RTC唤醒计数器的值
-------------------------------------------------------------------*/
uint32_t RTC_GetWakeUpCounter(void)
{
    /* Get the counter value */
    return ((uint32_t)(RTC->WKUPT & RTC_WKUPT_WKUPT));
}

/*-------------------------------------------------------------------
ErrorStatus RTC_EnableWakeUp(FunctionalState Cmd)允许/禁止RTC的唤醒功能
						ENABLE or DISABLE.
-------------------------------------------------------------------*/
ErrorStatus RTC_EnableWakeUp(FunctionalState Cmd)
{
    __IO uint32_t wutcounter = 0x00;
    uint32_t wutwfstatus     = 0x00;
    ErrorStatus status       = ERROR;

    /* Disable the write protection for RTC registers */
    RTC_EnableWriteProtection(DISABLE);

    if (Cmd != DISABLE)
    {
        /* Enable the Wakeup Timer */
        RTC->CTRL |= (uint32_t)RTC_CTRL_WTEN;
        status = SUCCESS;
    }
    else
    {
        /* Disable the Wakeup Timer */
        RTC->CTRL &= (uint32_t)~RTC_CTRL_WTEN;
        /* Wait till RTC WTWF flag is set and if Time out is reached exit */
        do
        {
            wutwfstatus = RTC->INITSTS & RTC_INITSTS_WTWF;
            wutcounter++;
        } while ((wutcounter != INITMODE_TIMEOUT) && (wutwfstatus == 0x00));

        if ((RTC->INITSTS & RTC_INITSTS_WTWF) == RESET)
        {
            status = ERROR;
        }
        else
        {
            status = SUCCESS;
        }
    }

    /* Enable the write protection for RTC registers */
    RTC_EnableWriteProtection(ENABLE);

    return status;
}

/*-------------------------------------------------------------------
void RTC_ConfigDayLightSaving(uint32_t RTC_DayLightSaving,uint32_t RTC_StoreOperation)
			设置RTC的夏时至的hour调整
			RTC_DayLightSaving = 	RTC_DAYLIGHT_SAVING_SUB1H Substract one hour (winter time).
														RTC_DAYLIGHT_SAVING_ADD1H Add one hour (summer time).
			RTC_StoreOperation = 	RTC_STORE_OPERATION_RESET 	BCK Bit Reset.
														RTC_STORE_OPERATION_SET 		BCK Bit Set.
-------------------------------------------------------------------*/
void RTC_ConfigDayLightSaving(uint32_t RTC_DayLightSaving, uint32_t RTC_StoreOperation)
{
    /* Disable the write protection for RTC registers */
    RTC_EnableWriteProtection(DISABLE);

    /* Clear the bits to be configured */
    RTC->CTRL &= (uint32_t) ~(RTC_CTRL_BAKP);

    /* Configure the RTC_CTRL register */
    RTC->CTRL |= (uint32_t)(RTC_DayLightSaving | RTC_StoreOperation);

    /* Enable the write protection for RTC registers */
    RTC_EnableWriteProtection(ENABLE);
}

/*-------------------------------------------------------------------
uint32_t RTC_GetStoreOperation(void)读取 夏时制的保持方式
				RTC_STORE_OPERATION_RESET
				RTC_STORE_OPERATION_SET
-------------------------------------------------------------------*/
uint32_t RTC_GetStoreOperation(void)
{
    return (RTC->CTRL & RTC_CTRL_BAKP);
}

/*-------------------------------------------------------------------
ErrorStatus RTC_ConfigSmoothCalib(uint32_t RTC_SmoothCalibPeriod,
                                  uint32_t RTC_SmoothCalibPlusPulses,
                                  uint32_t RTC_SmouthCalibMinusPulsesValue)
		设置RTC的平滑校准设置
		RTC_SmoothCalibPeriod = SMOOTH_CALIB_32SEC The smooth calibration periode is 32s.
														SMOOTH_CALIB_16SEC The smooth calibration periode is 16s.
														SMOOTH_CALIB_8SEC 	The smooth calibartion periode is 8s.
		RTC_SmoothCalibPlusPulses = RTC_SMOOTH_CALIB_PLUS_PULSES_SET 		Add one RTCCLK puls every 2**11 pulses.
																RTC_SMOOTH_CALIB_PLUS_PULSES__RESET No RTCCLK pulses are added.
		RTC_SmouthCalibMinusPulsesValue = 0 to 0x000001FF.
返回	SUCCESS: RTC Calib registers are configured
			ERROR: RTC Calib registers are not configured
-------------------------------------------------------------------*/
ErrorStatus RTC_ConfigSmoothCalib(uint32_t RTC_SmoothCalibPeriod,
                                  uint32_t RTC_SmoothCalibPlusPulses,
                                  uint32_t RTC_SmouthCalibMinusPulsesValue)
{
    ErrorStatus status    = ERROR;
    uint32_t recalpfcount = 0;

    /* Disable the write protection for RTC registers */
    RTC_EnableWriteProtection(DISABLE);

    /* check if a calibration is pending*/
    if ((RTC->INITSTS & RTC_INITSTS_RECPF) != RESET)
    {
        /* wait until the Calibration is completed*/
        while (((RTC->INITSTS & RTC_INITSTS_RECPF) != RESET) && (recalpfcount != RECALPF_TIMEOUT))
        {
            recalpfcount++;
        }
    }

    /* check if the calibration pending is completed or if there is no calibration operation at all*/
    if ((RTC->INITSTS & RTC_INITSTS_RECPF) == RESET)
    {
        /* Configure the Smooth calibration settings */
        RTC->CALIB = (uint32_t)((uint32_t)RTC_SmoothCalibPeriod | (uint32_t)RTC_SmoothCalibPlusPulses
                                | (uint32_t)RTC_SmouthCalibMinusPulsesValue);

        status = SUCCESS;
    }
    else
    {
        status = ERROR;
    }

    /* Enable the write protection for RTC registers */
    RTC_EnableWriteProtection(ENABLE);

    return (ErrorStatus)(status);
}

/*-------------------------------------------------------------------
ErrorStatus RTC_ConfigSynchroShift(uint32_t RTC_ShiftAdd1S,uint32_t RTC_ShiftSubFS)
			配置RTC的同步转换
 * @brief  Configures the Synchronization Shift Control Settings.
 * @note   When REFCKON is set, firmware must not write to Shift control register
 * @param RTC_ShiftAdd1S Select to add or not 1 second to the time Calendar.
 *   This parameter can be one of the following values :
 *     @arg RTC_SHIFT_ADD1S_ENABLE Add one second to the clock calendar.
 *     @arg RTC_SHIFT_ADD1S_DISABLE No effect.
 * @param RTC_ShiftSubFS Select the number of Second Fractions to Substitute.
 *         This parameter can be one any value from 0 to 0x7FFF.
 * @return An ErrorStatus enumeration value:
 *          - SUCCESS: RTC Shift registers are configured
 *          - ERROR: RTC Shift registers are not configured
-------------------------------------------------------------------*/
ErrorStatus RTC_ConfigSynchroShift(uint32_t RTC_ShiftAdd1S, uint32_t RTC_ShiftSubFS)
{
    ErrorStatus status = ERROR;
    uint32_t shpfcount = 0;

    /* Disable the write protection for RTC registers */
    RTC_EnableWriteProtection(DISABLE);

    /* Check if a Shift is pending*/
    if ((RTC->INITSTS & RTC_INITSTS_SHOPF) != RESET)
    {
        /* Wait until the shift is completed*/
        while (((RTC->INITSTS & RTC_INITSTS_SHOPF) != RESET) && (shpfcount != SHPF_TIMEOUT))
        {
            shpfcount++;
        }
    }

    /* Check if the Shift pending is completed or if there is no Shift operation at all*/
    if ((RTC->INITSTS & RTC_INITSTS_SHOPF) == RESET)
    {
        /* Configure the Shift settings */
        RTC->SCTRL = (uint32_t)(uint32_t)(RTC_ShiftSubFS) | (uint32_t)(RTC_ShiftAdd1S);

        if (RTC_WaitForSynchro() == ERROR)
        {
            status = ERROR;
        }
        else
        {
            status = SUCCESS;
        }       
    }
    else
    {
        status = ERROR;
    }

    /* Enable the write protection for RTC registers */
    RTC_EnableWriteProtection(ENABLE);

    return (ErrorStatus)(status);
}

/*-------------------------------------------------------------------
void RTC_ConfigInt(uint32_t RTC_INT, FunctionalState Cmd)
		配置RTC的中断
		RTC_INT = RTC_INT_WUT 	WakeUp Timer interrupt mask.
							RTC_INT_ALRA 	Alarm A interrupt mask.
		ENABLE or DISABLE.
-------------------------------------------------------------------*/
void RTC_ConfigInt(uint32_t RTC_INT, FunctionalState Cmd)
{
    /* Disable the write protection for RTC registers */
    RTC_EnableWriteProtection(DISABLE);

    if (Cmd != DISABLE)
    {
        /* Configure the Interrupts in the RTC_CTRL register */
        RTC->CTRL |= (uint32_t)RTC_INT;
    }
    else
    {
        /* Configure the Interrupts in the RTC_CTRL register */
        RTC->CTRL &= (uint32_t) ~(RTC_INT);
    }
    /* Enable the write protection for RTC registers */
    RTC_EnableWriteProtection(ENABLE);
}

/*-------------------------------------------------------------------
FlagStatus RTC_GetFlagStatus(uint32_t RTC_FLAG)读取RTC的标志位
	RTC_FLAG = 	RTC_FLAG_RECPF 	RECALPF event flag.
							RTC_FLAG_WTF 		WakeUp Timer flag.
							RTC_FLAG_ALAF 	Alarm A flag.
							RTC_FLAG_INITF 	Initialization mode flag.
							RTC_FLAG_RSYF 	Registers Synchronized flag.
							RTC_FLAG_INITSF Registers Configured flag.
							RTC_FLAG_SHOPF 	Shift operation pending flag.
							RTC_FLAG_WTWF 	WakeUp Timer Write flag.
							RTC_FLAG_ALAWF 	Alarm A write flag.
SET or RESET).
-------------------------------------------------------------------*/
FlagStatus RTC_GetFlagStatus(uint32_t RTC_FLAG)
{
    FlagStatus bitstatus = RESET;
    uint32_t tmpregister = 0;

    /* Get all the flags */
    tmpregister = (uint32_t)(RTC->INITSTS & RTC_FLAGS_MASK);

    /* Return the status of the flag */
    if ((tmpregister & RTC_FLAG) != (uint32_t)RESET)
    {
        bitstatus = SET;
    }
    else
    {
        bitstatus = RESET;
    }
    return bitstatus;
}

/*-------------------------------------------------------------------
void RTC_ClrFlag(uint32_t RTC_FLAG)清除RTC标志位
	RTC_FLAG = 	RTC_FLAG_WTF 		WakeUp Timer flag.
							RTC_FLAG_ALAF 	Alarm A flag.
							RTC_FLAG_RSYF 	Registers Synchronized flag.
-------------------------------------------------------------------*/
void RTC_ClrFlag(uint32_t RTC_FLAG)
{

    /* Clear the Flags in the RTC_INITSTS register */
    RTC->INITSTS = (uint32_t)(
        (uint32_t)(~((RTC_FLAG | RTC_INITSTS_INITM) & 0x0001FFFF) | (uint32_t)(RTC->INITSTS & RTC_INITSTS_INITM)));
}

/*-------------------------------------------------------------------
INTStatus RTC_GetITStatus(uint32_t RTC_INT)读取RTC的中断标志
	RTC_INT = RTC_INT_WUT 	WakeUp Timer interrupt.
						RTC_INT_ALRA 	Alarm A interrupt.
 SET or RESET).
-------------------------------------------------------------------*/
INTStatus RTC_GetITStatus(uint32_t RTC_INT)
{
    INTStatus bitstatus  = RESET;
    uint32_t tmpregister = 0, enablestatus = 0;

     
    enablestatus = (uint32_t)((RTC->CTRL & RTC_INT));

    /* Get the Interrupt pending bit */
    tmpregister = (uint32_t)((RTC->INITSTS & (uint32_t)(RTC_INT >> 4)));

    /* Get the status of the Interrupt */
    if ((enablestatus != (uint32_t)RESET) && ((tmpregister & 0x0000FFFF) != (uint32_t)RESET))
    {
        bitstatus = SET;
    }
    else
    {
        bitstatus = RESET;
    }
    return bitstatus;
}

/*-------------------------------------------------------------------
void RTC_ClrIntPendingBit(uint32_t RTC_INT)清除RTC中断标志
	RTC_INT = RTC_INT_WUT 		WakeUp Timer interrupt
						RTC_INT_ALRA 		Alarm A interrupt
-------------------------------------------------------------------*/
void RTC_ClrIntPendingBit(uint32_t RTC_INT)
{
    uint32_t tmpregister = 0;

    /* Get the RTC_INITSTS Interrupt pending bits mask */
    tmpregister = (uint32_t)(RTC_INT >> 4);

    /* Clear the interrupt pending bits in the RTC_INITSTS register */
    RTC->INITSTS = (uint32_t)(
        (uint32_t)(~((tmpregister | RTC_INITSTS_INITM) & 0x0000FFFF) | (uint32_t)(RTC->INITSTS & RTC_INITSTS_INITM)));
}

/*-------------------------------------------------------------------
static uint8_t RTC_ByteToBcd2(uint8_t Value)
 * @brief  Converts a 2 digit decimal to BCD format.
 * @param Value Byte to be converted.
 * @return Converted byte
-------------------------------------------------------------------*/
static uint8_t RTC_ByteToBcd2(uint8_t Value)
{
    uint8_t bcdhigh = 0;

    while (Value >= 10)
    {
        bcdhigh++;
        Value -= 10;
    }

    return ((uint8_t)(bcdhigh << 4) | Value);
}

/*-------------------------------------------------------------------
static uint8_t RTC_Bcd2ToByte(uint8_t Value)
 * @brief  Convert from 2 digit BCD to Binary.
 * @param Value BCD value to be converted.
 * @return Converted word
-------------------------------------------------------------------*/
static uint8_t RTC_Bcd2ToByte(uint8_t Value)
{
    uint8_t tmp = 0;
    tmp         = ((uint8_t)(Value & (uint8_t)0xF0) >> (uint8_t)0x4) * 10;
    return (tmp + (Value & (uint8_t)0x0F));
}


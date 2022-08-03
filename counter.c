/*
 * counter.c
 * Copyright (c) 2021-22 Andre M. Maree / KSS Technologies (Pty) Ltd.
 */

#include "counter.h"
#include "printfx.h"
#include "definitions.h"

/* Design notes:
 * -------------
 * used for pulse counters, not scalar value sensors.
 * All values to be stored in non volatile memory
 * At startup all buckets are 0.
 * Pulses are counted into the NOW bucket, interrupt driven.
 *
 * Proposed logic:
 * At HH:MM:00 NOW bucket into indexed MINS bucket, clear NOW bucket.
 * At HH:00:00 every hour, sum of MINS buckets into indexed HOUR bucket
 * At 00:00:00 every day, sum of HOUR buckets into indexed DAY bucket
 * At 00:00:00 of 1st of month, sum of DAY buckets into indexed MONTH bucket
 * At 00:00:00 on 1st January, sum of MONTH buckets into YEAR bucket.
 *
 * Alternative/additional logic:
 * To make more responsive maintain HourTD, DayTD, MonthTD and YearTD counters.
 * Reset XTD counters hourly, midnight daily/monthly/newyear basis
 * Update XTD counters either:
 *	a)	all counters with every pulse coming in; or
 *	b)	NOW counter with every pulse coming in, and
 * 		every minute NOW counter added to all XTD counters, and
 *		continue to follow proposed logic above for historic detail purposes.
 */

// ###################################### Local build macros #######################################

#define	debugFLAG					0xF000

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// ########################################## Structures ###########################################

typedef struct __attribute__((packed)) {
	uint8_t		MinTD,	Min[MINUTES_IN_HOUR] ;
	uint8_t 	HourTD, Hour[HOURS_IN_DAY] ;
	uint16_t	DayTD,	Day[DAYS_IN_MONTH_MAX] ;
	uint16_t	MonTD,	Mon[MONTHS_IN_YEAR] ;
	uint32_t	YearTD,	Year ;
} pulsecnt_t ;

// ####################################### Public variables ########################################


// ####################################### Private variables #######################################

pulsecnt_t * psPCdata ;
static int LastMin = -1 ;
static uint8_t pcntNumCh;

// ########################################### Public functions ####################################

int xPulseCountInit(int NumCh) {
	if (OUTSIDE(0, NumCh, 255))
		return erFAILURE;
	pcntNumCh = NumCh ;
	psPCdata = pvRtosMalloc(NumCh * sizeof(pulsecnt_t)) ;
	return erSUCCESS;
}

/**
 * Update all fields in the basic counter structure.
 * @param 	psTM	Current time structure
 * @return	-1 = repeat call this minute, 0 = normal update, 1 = month end update
 */
int	xPulseCountUpdate(struct tm * psTM) {
	if (psTM->tm_sec != 0 || psTM->tm_min == LastMin)
		return -1; 										// ??:??:00, once only..
	LastMin = psTM->tm_min ;
	int iRV = 0 ;										// default for "NORMAL" update
	for (int i = 0; i < pcntNumCh; ++i) {
		pulsecnt_t * psPC = psPCdata[i] ;
		psPC->Min[psTM->tm_min]	= psPC->MinTD ;			// persist last minute
		psPC->MinTD = 0 ;

		if (psTM->tm_min == 0) {						// 0 -> 59
			psPC->Hour[psTM->tm_hour] = psPC->HourTD ;	// persist last hour
			psPC->HourTD = 0 ;
		} else if (psTM->tm_min == 59 &&
					psTM->tm_hour == 23 &&
					psTM->tm_mday == xTimeCalcDaysInMonth(psTM)) {
			/* At this point we are at 23:59.00 of the last day in this calendar month
			 * In order have averages correct ZERO remaining (not in month) array days */
			for (int i = psTM->tm_mday; i < DAYS_IN_MONTH_MAX; psPC->Day[i] = 0, ++i) ;
			iRV = 1 ; 									// special "MONTHEND" update
		} else {
			continue ;
		}

		if (psTM->tm_hour != 0)
			continue;									// 0 -> 23
		psPC->Day[psTM->tm_mday-1] = psPC->DayTD ;		// persist last day (make 0 relative)
		psPC->DayTD = 0 ;

		if (psTM->tm_mday != 1)
			continue;									// 1 -> 31
		psPC->Mon[psTM->tm_mon] = psPC->MonTD ;			// persist last month
		psPC->MonTD = 0 ;

		if (psTM->tm_mon != 0)
			continue;									// 0 -> 11
		psPC->Year = psPC->YearTD ;						// persist last year
		psPC->YearTD = 0 ;
	}
	return iRV ;
}

int	xPulseCountIncrement(int Idx) {
	if (OUTSIDE(0, Idx, pcntNumCh))
		return erFAILURE;
	pulsecnt_t * psPC = psPCdata[Idx] ;
	psPC->MinTD++ ;
	IF_PL(psPC->MinTD == 0, "Wrapped, Pulse rate too high\r\n") ;
	psPC->HourTD++ ;
	psPC->DayTD++ ;
	psPC->MonTD++ ;
	psPC->YearTD++ ;
	return erSUCCESS;
}

void vPulseCountReport(void) {
	struct tm sTM ;
	xTimeGMTime(xTimeStampAsSeconds(sTSZ.usecs), &sTM, 0) ;
	for (int i = 0; i < pcntNumCh; ++i) {
		pulsecnt_t * psPC = psPCdata[i] ;
		printfx("%d: MinTD=%u  HourTD=%u  DayTD=%u  MonTD=%u  YearTD=%u\r\n",
				i, psPC->MinTD, psPC->HourTD, psPC->DayTD, psPC->MonTD, psPC->YearTD) ;
		printfx("Min :  ") ;
		for (int j = 0; j < MINUTES_IN_HOUR; ++j) {
			uint32_t Col = (j == sTM.tm_min) ? xpfSGR(colourFG_CYAN,0,0,0) : xpfSGR(attrRESET,0,0,0) ;
			printfx("%C%u%C  ", Col, psPC->Min[j], xpfSGR(attrRESET,0,0,0)) ;
		}
		printfx("\r\nHour:  ") ;
		for (int j = 0; j < HOURS_IN_DAY; ++j) {
			uint32_t Col = (j == sTM.tm_hour) ? xpfSGR(colourFG_CYAN,0,0,0) : xpfSGR(attrRESET,0,0,0) ;
			printfx("%C%u%C  ", Col, psPC->Hour[j], xpfSGR(attrRESET,0,0,0)) ;
		}
		printfx("\r\nDay :  ") ;
		for (int j = 0; j < DAYS_IN_MONTH_MAX; ++j) {
			uint32_t Col = (j == sTM.tm_mday) ? xpfSGR(colourFG_CYAN,0,0,0) : xpfSGR(attrRESET,0,0,0) ;
			printfx("%C%u%C  ", Col, psPC->Day[j], xpfSGR(attrRESET,0,0,0)) ;
		}
		printfx("\r\nMon :  ") ;
		for (int j = 0; j < MONTHS_IN_YEAR; ++j) {
			uint32_t Col = (j == sTM.tm_mon) ? xpfSGR(colourFG_CYAN,0,0,0) : xpfSGR(attrRESET,0,0,0) ;
			printfx("%C%u%C  ", Col, psPC->Mon[j], xpfSGR(attrRESET,0,0,0)) ;
		}
		printfx("\r\nYear:  %u\r\n\n", psPC->Year) ;
	}
}

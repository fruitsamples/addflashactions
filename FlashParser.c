//////////////	File:		FlashParser.c////	Contains:	Sample code for .////	Written by:	////	Copyright:	� 1999 by Apple Computer, Inc., all rights reserved.////	Change History (most recent first):////////////#include "FlashParser.h"/*__________________________________________________________________________________________QuickTime Mouse Event to Flash Mouse Event Mappings:	kQTEventMouseClick		bsOverUpToOverDown			kQTEventMouseClickEnd		if ( stagDefineButton2:trackAsMenu == false )			bsOutDownToIdle		kQTEventMouseClickEndTriggerButton		bsOverDownToOverUp			kQTEventMouseEnter		bsIdleToOverUp		if ( stagDefineButton2:trackAsMenu == true )			bsIdleToOverDown		else			bsOutDownToOverDown			kQTEventMouseExit		bsOverUpToIdle		if ( stagDefineButton2:trackAsMenu == true )			bsOverDownToIdle		else			bsOverDownToOutDown__________________________________________________________________________________________*/FlashParserStruct		gFlashParserData;	/*	GetByte		Extract one byte from the data stream*/inline U8 GetByte () {    return gFlashParserData.m_fileBuf [gFlashParserData.m_filePos++];}/*	GetWord		Extract and byte flip one word from the data stream*/inline U16 GetWord (){    U8 * s = gFlashParserData.m_fileBuf + gFlashParserData.m_filePos;    gFlashParserData.m_filePos += 2;    return (U16) s[0] | ((U16) s[1] << 8);}/*	GetDWord 		Extract and byte flip one long word from the data stream*/inline U32 GetDWord (){    U8 * s = gFlashParserData.m_fileBuf + gFlashParserData.m_filePos;    gFlashParserData.m_filePos += 4;    return (U32) s[0] | ((U32) s[1] << 8) | ((U32) s[2] << 16) | ((U32) s [3] << 24);}char * GetAString(){    // Point to the string.    char *str = (char *) &gFlashParserData.m_fileBuf[gFlashParserData.m_filePos];    // Skip over the string.    while (GetByte());    return str;}/*	InitBits	    Reset the bit position and buffer.*/void InitBits(){    gFlashParserData.m_bitPos = 0;    gFlashParserData.m_bitBuf = 0;}U32 GetBits (S32 n){    U32 v = 0;    for (;;)    {        S32 s = n - gFlashParserData.m_bitPos;        if (s > 0)        {            // Consume the entire buffer            v |= gFlashParserData.m_bitBuf << s;            n -= gFlashParserData.m_bitPos;            // Get the next buffer            gFlashParserData.m_bitBuf = GetByte();            gFlashParserData.m_bitPos = 8;        }        else        {         	// Consume a portion of the buffer            v |= gFlashParserData.m_bitBuf >> -s;            gFlashParserData.m_bitPos -= n;            gFlashParserData.m_bitBuf &= 0xff >> (8 - gFlashParserData.m_bitPos);	// mask off the consumed bits            return v;        }    }}/*	GetSBits		Get n bits from the string with sign extension.*/S32 GetSBits (S32 n){    // Get the number as an unsigned value.    S32 v = (S32) GetBits(n);    // Is the number negative?    if (v & (1L << (n - 1)))    {        // Yes. Extend the sign.        v |= -1L << n;    }    return v;}void GetRect (SRECT * r){	int nBits;	 	     InitBits ();        nBits = (int) GetBits(5);    r->xmin = GetSBits(nBits);    r->xmax = GetSBits(nBits);    r->ymin = GetSBits(nBits);    r->ymax = GetSBits(nBits);}/*	GetTag		Extract tag id and tag length from the data stream*/U16 GetTag (){	U16 code;	U32 len;	    // Save the start of the tag.	gFlashParserData.m_tagStart = gFlashParserData.m_filePos;        // Get the combined code and length of the tag.	code = GetWord ();    // The length is encoded in the tag.	len = code & 0x3f;    // Remove the length from the code.    code = code >> 6;    // Determine if another long word must be read to get the length.    if (len == 0x3f) len = (U32) GetDWord ();    // Determine the end position of the tag.    gFlashParserData.m_tagEnd = gFlashParserData.m_filePos + (U32) len;    gFlashParserData.m_tagLen = (U32) len;    return code;}/*	SetNewHeaderLengthTagLength		Modifies the tag length portion of button tag to reflect change in size of tag, and stream header filelength		If the length of the data is greater than a byte, an additional long is required to 	store the data length in the tag.*/void SetNewHeaderLengthTagLength (U32 fileDifference, U32 tagDifference){	OSErr		theErr;		U16 		code,				newCode;		U8 * 		s;        U32 		length,    			fileLength;         long		handleSize;    	Boolean		wasDouble = false;	        HLock (gFlashParserData.m_theData);	s = (U8 *) *gFlashParserData.m_theData + 4;			fileLength = (U32) s[0] | ((U32) s[1] << 8) | ((U32) s[2] << 16) | ((U32) s [3] << 24);	fileLength += fileDifference;	s [0] = (fileLength & 0xFF);	s [1] = ((fileLength >> 8) & 0xFF);	s [2] = ((fileLength >> 16) & 0xFF);	s [3] = ((fileLength >> 24) & 0xFF);           s = (U8 *) *gFlashParserData.m_theData + gFlashParserData.m_tagStart;	// Get the combined code and length of the tag.	code = (U16) s[0] | ((U16) s[1] << 8);    // The length is encoded in the tag.	length = code & 0x3f;    // Remove the length from the code.    code = code >> 6;    // Determine if another long word must be read to get the length.    if (length == 0x3f)    {    	s += sizeof (U16);    	    	length = (U32) s[0] | ((U32) s[1] << 8) | ((U32) s[2] << 16) | ((U32) s [3] << 24);    	wasDouble = true;    }		HUnlock (gFlashParserData.m_theData);		length += tagDifference;		if (length >= 0x3F)	{		newCode = (code << 6) | 0x3F;					if (!wasDouble)		// need more space		{			handleSize = GetHandleSize (gFlashParserData.m_theData);						handleSize += sizeof (long);						SetHandleSize (gFlashParserData.m_theData, handleSize);			theErr = MemError ();			if (theErr != noErr)				goto Bail;						// now shift the data up						HLock (gFlashParserData.m_theData);						BlockMove (*gFlashParserData.m_theData + gFlashParserData.m_tagStart, *gFlashParserData.m_theData + gFlashParserData.m_tagStart + sizeof (long), 								(handleSize - sizeof (long)) - gFlashParserData.m_tagStart);			HUnlock (gFlashParserData.m_theData);						fileLength += sizeof (long);		}					HLock (gFlashParserData.m_theData);				s = (U8 *) *gFlashParserData.m_theData + 4;		s [0] = (fileLength & 0xFF);		s [1] = ((fileLength >> 8) & 0xFF);		s [2] = ((fileLength >> 16) & 0xFF);		s [3] = ((fileLength >> 24) & 0xFF);						s = (U8 *) *gFlashParserData.m_theData + gFlashParserData.m_tagStart;				s [0] = (newCode & 0xFF);		s [1] = ((newCode >> 8) & 0xFF);				s += sizeof (U16);		s [0] = (length & 0xFF);		s [1] = ((length >> 8) & 0xFF);		s [2] = ((length >> 16) & 0xFF);		s [3] = ((length >> 24) & 0xFF);				HUnlock (gFlashParserData.m_theData);	}	else	{		newCode = (U16) (code << 6) | (U16) (length & 0x3F);		if (wasDouble)		{			handleSize = GetHandleSize (gFlashParserData.m_theData);						handleSize -= sizeof (long);						// shift the data down						HLock (gFlashParserData.m_theData);						BlockMove (*gFlashParserData.m_theData + gFlashParserData.m_tagStart + sizeof (long), *gFlashParserData.m_theData + gFlashParserData.m_tagStart,								(handleSize - sizeof (long)) - gFlashParserData.m_tagStart);			HUnlock (gFlashParserData.m_theData);						SetHandleSize (gFlashParserData.m_theData, handleSize);			theErr = MemError ();			if (theErr != noErr)				goto Bail;			fileLength -= sizeof (long);		}		HLock (gFlashParserData.m_theData);		s = (U8 *) *gFlashParserData.m_theData + 4;		s [0] = (fileLength & 0xFF);		s [1] = ((fileLength >> 8) & 0xFF);		s [2] = ((fileLength >> 16) & 0xFF);		s [3] = ((fileLength >> 24) & 0xFF);		s = (U8 *) *gFlashParserData.m_theData + gFlashParserData.m_tagStart;				s [0] = (newCode & 0xFF);		s [1] = ((newCode >> 8) & 0xFF);		HUnlock (gFlashParserData.m_theData);	}	Bail:	return;}/*	ParseTags		Parses the tags within the file.*/void ParseTags (Boolean sprite, long *buttonID){ 	BOOL 	atEnd; 	   	U16		code;	U32		tagEnd; 	   	      if (sprite)    {        U32 tagid = (U32) GetWord ();        U32 frameCount = (U32) GetWord ();    }    else    {                // Set the position to the start position.        gFlashParserData.m_filePos = gFlashParserData.m_fileStart;    }                // Initialize the end of frame flag.	atEnd = false;    // Loop through each tag.    while (!atEnd)    {        // Get the current tag.		code = GetTag();        // Get the tag ending position.		tagEnd = gFlashParserData.m_tagEnd;        switch (code)        {            case stagEnd:                // We reached the end of the file.                atEnd = true;                break;				            case stagDefineButton2:                *buttonID = (U32) GetWord();                atEnd = true;                break;            case stagDefineSprite:                ParseTags (true, buttonID);                break;            default:                 break;        }        // Increment past the tag.   	    gFlashParserData.m_filePos = tagEnd;    }}/*	GetOffsetForButton		Returns offset in data stream of button data of buttonID*/U32 GetOffsetForButton (long buttonID){	SRECT 		rect;	U32			frameRate,				frameCount;	    BOOL 		atEnd = false;    U32 		frame = 0,    	 		tagid;		gFlashParserData.m_fileBuf = (U8 *) *gFlashParserData.m_theData;		// Set the file position past the header and size information.    gFlashParserData.m_filePos = 8;	GetRect (&rect);	frameRate = GetWord () >> 8;	frameCount = GetWord ();		gFlashParserData.m_fileStart = gFlashParserData.m_filePos;			//	ParseTags (false, 0);    // Loop through each tag.    while (!atEnd)    {        // Get the current tag.        U16 code = GetTag();        // Get the tag ending position.        U32 tagEnd = gFlashParserData.m_tagEnd;       	switch (code)        {            case stagEnd:                // We reached the end of the file.                atEnd = true;                break;                    case stagDefineButton2:                tagid = (U32) GetWord ();                if (tagid == buttonID)                {                	return gFlashParserData.m_tagStart;                }                break;            default:                 break;        }        // Increment past the tag.        gFlashParserData.m_filePos = tagEnd;    }			return 0;}/*	GetOffsetForFrame		Returns offset in data stream of frame of frameID		Zero based*/void GetOffsetForFrame (long frameID, U32 *frameStart, U32 *frameEnd){	SRECT 		rect;	U32			frameRate,				frameCount,				lastFrameFilePos;    U32 		frame = 0;    	 //		tagid;    BOOL 		atEnd = false;		gFlashParserData.m_fileBuf = (U8 *) *gFlashParserData.m_theData;		// Set the file position past the header and size information.    gFlashParserData.m_filePos = 8;	GetRect (&rect);	frameRate = GetWord () >> 8;	frameCount = GetWord ();		gFlashParserData.m_fileStart = gFlashParserData.m_filePos;	lastFrameFilePos = gFlashParserData.m_fileStart;	    // Loop through each tag.    while (!atEnd)    {        // Get the current tag.        U16 code = GetTag();        // Get the tag ending position.        U32 tagEnd = gFlashParserData.m_tagEnd;       	switch (code)        {            case stagEnd:                // We reached the end of the file.                atEnd = true;                break;                	case stagShowFrame:        		if (frame == frameID)        		{        			*frameStart = lastFrameFilePos;        			*frameEnd = gFlashParserData.m_tagStart;        			        			return;        		}        	                lastFrameFilePos = gFlashParserData.m_tagStart;                ++frame;        	        		break;        		            default:                 break;        }        // Increment past the tag.        gFlashParserData.m_filePos = tagEnd;    }			if (frameCount == 1)	{		*frameStart = gFlashParserData.m_fileStart;		*frameEnd = gFlashParserData.m_tagStart;	}	else	{		*frameStart = 0;		*frameEnd = 0;	}}void Initialize (void){    // Initialize the input pointer.	gFlashParserData.m_fileBuf = NULL;    // Initialize the file information.	gFlashParserData.m_filePos = 0;	gFlashParserData.m_fileSize = 0;	gFlashParserData.m_fileStart = 0;	gFlashParserData.m_fileVersion = 0;    // Initialize the bit position and buffer.	gFlashParserData.m_bitPos = 0;	gFlashParserData.m_bitBuf = 0;}/*	Locate the first button in the data stream*/OSErr LocateFirstButton (Handle theSample, long *buttonID){	OSErr		theErr = noErr;		SRECT 		rect;	U32			frameRate,				frameCount;		*buttonID = 0;	Initialize ();	gFlashParserData.m_theData = theSample;	HLock (theSample);	gFlashParserData.m_fileBuf = (U8 *) *theSample;		// Set the file position past the header and size information.    gFlashParserData.m_filePos = 8;	GetRect (&rect);	frameRate = GetWord () >> 8;	frameCount = GetWord ();		gFlashParserData.m_fileStart = gFlashParserData.m_filePos;		ParseTags (false, buttonID);		HUnlock (theSample);	return theErr;}
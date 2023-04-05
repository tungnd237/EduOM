/******************************************************************************/
/*                                                                            */
/*    ODYSSEUS/EduCOSMOS Educational-Purpose Object Storage System            */
/*                                                                            */
/*    Developed by Professor Kyu-Young Whang et al.                           */
/*                                                                            */
/*    Database and Multimedia Laboratory                                      */
/*                                                                            */
/*    Computer Science Department and                                         */
/*    Advanced Information Technology Research Center (AITrc)                 */
/*    Korea Advanced Institute of Science and Technology (KAIST)              */
/*                                                                            */
/*    e-mail: kywhang@cs.kaist.ac.kr                                          */
/*    phone: +82-42-350-7722                                                  */
/*    fax: +82-42-350-8380                                                    */
/*                                                                            */
/*    Copyright (c) 1995-2013 by Kyu-Young Whang                              */
/*                                                                            */
/*    All rights reserved. No part of this software may be reproduced,        */
/*    stored in a retrieval system, or transmitted, in any form or by any     */
/*    means, electronic, mechanical, photocopying, recording, or otherwise,   */
/*    without prior written permission of the copyright owner.                */
/*                                                                            */
/******************************************************************************/
/*
 * Module : EduOM_CreateObject.c
 * 
 * Description :
 *  EduOM_CreateObject() creates a new object near the specified object.
 *
 * Exports:
 *  Four EduOM_CreateObject(ObjectID*, ObjectID*, ObjectHdr*, Four, char*, ObjectID*)
 */

#include <string.h>
#include "EduOM_common.h"
#include "RDsM.h"		/* for the raw disk manager call */
#include "BfM.h"		/* for the buffer manager call */
#include "EduOM_Internal.h"

/*@================================
 * EduOM_CreateObject()
 *================================*/
/*
 * Function: Four EduOM_CreateObject(ObjectID*, ObjectID*, ObjectHdr*, Four, char*, ObjectID*)
 * 
 * Description :
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 * (1) What to do?
 * EduOM_CreateObject() creates a new object near the specified object.
 * If there is no room in the page holding the specified object,
 * it trys to insert into the page in the available space list. If fail, then
 * the new object will be put into the newly allocated page.
 *
 * (2) How to do?
 *	a. Read in the near slotted page
 *	b. See the object header
 *	c. IF large object THEN
 *	       call the large object manager's lom_ReadObject()
 *	   ELSE 
 *		   IF moved object THEN 
 *				call this function recursively
 *		   ELSE 
 *				copy the data into the buffer
 *		   ENDIF
 *	   ENDIF
 *	d. Free the buffer page
 *	e. Return
 *
 * Returns:
 *  error code
 *    eBADCATALOGOBJECT_OM
 *    eBADLENGTH_OM
 *    eBADUSERBUF_OM
 *    some error codes from the lower level
 *
 * Side Effects :
 *  0) A new object is created.
 *  1) parameter oid
 *     'oid' is set to the ObjectID of the newly created object.
 */
Four EduOM_CreateObject(
    ObjectID  *catObjForFile,	/* IN file in which object is to be placed */
    ObjectID  *nearObj,		/* IN create the new object near this object */
    ObjectHdr *objHdr,		/* IN from which tag is to be set */
    Four      length,		/* IN amount of data */
    char      *data,		/* IN the initial data for the object */
    ObjectID  *oid)		/* OUT the object's ObjectID */
{
    Four        e;		/* error number */
    ObjectHdr   objectHdr;	/* ObjectHdr with tag set from parameter */


    /*@ parameter checking */
    
    if (catObjForFile == NULL) ERR(eBADCATALOGOBJECT_OM);
    
    if (length < 0) ERR(eBADLENGTH_OM);

    if (length > 0 && data == NULL) return(eBADUSERBUF_OM);

        /* Error check whether using not supported functionality by EduOM */
        if(ALIGNED_LENGTH(length) > LRGOBJ_THRESHOLD) ERR(eNOTSUPPORTED_EDUOM);
    
    objectHdr.length = 0;
    objectHdr.tag = 0;
    if (objHdr != NULL) {
        objectHdr.tag = objHdr->tag;
    }
    objectHdr.properties = 0x0;
   
    /* Call eduom_CreateObject() to insert an object into the page */
    e = eduom_CreateObject(catObjForFile, nearObj, &objectHdr, length, data, oid);
    if(e < 0) ERR(e);
    
    return(eNOERROR);
}
/*@================================
 * eduom_CreateObject()
 *================================*/
/*
 * Function: Four eduom_CreateObject(ObjectID*, ObjectID*, ObjectHdr*, Four, char*, ObjectID*)
 *
 * Description :
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 *  eduom_CreateObject() creates a new object near the specified object; the near
 *  page is the page holding the near object.
 *  If there is no room in the near page and the near object 'nearObj' is not
 *  NULL, a new page is allocated for object creation (In this case, the newly
 *  allocated page is inserted after the near page in the list of pages
 *  consiting in the file).
 *  If there is no room in the near page and the near object 'nearObj' is NULL,
 *  it trys to create a new object in the page in the available space list. If
 *  fail, then the new object will be put into the newly allocated page(In this
 *  case, the newly allocated page is appended at the tail of the list of pages
 *  cosisting in the file).
 *
 * Returns:
 *  error Code
 *    eBADCATALOGOBJECT_OM
 *    eBADOBJECTID_OM
 *    some errors caused by fuction calls
 */
Four eduom_CreateObject(
                        ObjectID	*catObjForFile,	/* IN file in which object is to be placed */
                        ObjectID 	*nearObj,	/* IN create the new object near this object */
                        ObjectHdr	*objHdr,	/* IN from which tag & properties are set */
                        Four	length,		/* IN amount of data */
                        char	*data,		/* IN the initial data for the object */
                        ObjectID	*oid)		/* OUT the object's ObjectID */
{
    Four        e;		/* error number */
    Four	neededSpace;	/* space needed to put new object [+ header] */
    SlottedPage *apage;		/* pointer to the slotted page buffer */
    Four        alignedLen;	/* aligned length of initial data */
    Boolean     needToAllocPage;/* Is there a need to alloc a new page? */
    PageID      pid;            /* PageID in which new object to be inserted */
    PageID      nearPid;
    Four        firstExt;	/* first Extent No of the file */
    Object      *obj;		/* point to the newly created object */
    Two         i;		/* index variable */
    sm_CatOverlayForData *catEntry; /* pointer to data file catalog information */
    SlottedPage *catPage;	/* pointer to buffer containing the catalog */
    FileID      fid;		/* ID of file where the new object is placed */
    Two         eff;		/* extent fill factor of file */
    Boolean     isTmp;
    PhysicalFileID pFid;
    Boolean pageSelected;
    
    /*@ parameter checking */
    
    if (catObjForFile == NULL) ERR(eBADCATALOGOBJECT_OM);
    
    if (objHdr == NULL) ERR(eBADOBJECTID_OM);
    
        /* Error check whether using not supported functionality by EduOM */
        if(ALIGNED_LENGTH(length) > LRGOBJ_THRESHOLD) ERR(eNOTSUPPORTED_EDUOM);

    /* get the buffer containing the catalog */
    e = BfM_GetTrain((TrainID*)catObjForFile, (char**)&catPage, PAGE_BUF);
    if (e < 0) ERR( e );

    GET_PTR_TO_CATENTRY_FOR_DATA(catObjForFile, catPage, catEntry);

    fid = catEntry->fid;
    pFid.pageNo = catEntry->firstPage;
    pFid.volNo = catEntry->fid.volNo;
    

    alignedLen = ALIGNED_LENGTH(length);   
    neededSpace = alignedLen + sizeof(SlottedPageSlot) + sizeof(ObjectHdr); 
    
    pageSelected = FALSE;

    /* select the page to insert the object */
    if (nearObj == NULL) {
        if (pageSelected == FALSE) {
            pid.pageNo = catEntry->lastPage;
            pid.volNo = catEntry->fid.volNo;
        } else if (neededSpace <= sizeof(catPage->data) * 1 / 10 && pageSelected == FALSE && catEntry->availSpaceList10 >= 0) {
            pageSelected = TRUE;
            pid.pageNo = catEntry->availSpaceList10;
            pid.volNo = catEntry->fid.volNo;
        } else if (neededSpace <= sizeof(catPage->data) * 2 / 10 && pageSelected == FALSE && catEntry->availSpaceList20 >= 0) {
            pageSelected = TRUE;
            pid.pageNo = catEntry->availSpaceList20;
            pid.volNo = catEntry->fid.volNo;
        } else if (neededSpace <= sizeof(catPage->data) * 3 / 10 && pageSelected == FALSE && catEntry->availSpaceList30 >= 0) {
            pageSelected = TRUE;
            pid.pageNo = catEntry->availSpaceList30;
            pid.volNo = catEntry->fid.volNo;
            
        } else if (neededSpace <= sizeof(catPage->data) * 4 / 10 && pageSelected == FALSE && catEntry->availSpaceList40 >= 0) {
            pageSelected = TRUE;
            pid.pageNo = catEntry->availSpaceList40;
            pid.volNo = catEntry->fid.volNo;
        } else if (neededSpace <= sizeof(catPage->data) * 5 / 10 && pageSelected == FALSE && catEntry->availSpaceList50 >= 0) {
            pageSelected = TRUE;
            pid.pageNo = catEntry->availSpaceList50;
            pid.volNo = catEntry->fid.volNo;
        }
    } else {
        pid.pageNo = nearObj->pageNo;
        pid.volNo = nearObj->volNo;
        pageSelected = TRUE;
    }


    e = BfM_GetTrain(&pid, &apage, PAGE_BUF);
    if (e < 0) ERR(e);

    if (SP_FREE(apage) >= neededSpace) {
        e = om_RemoveFromAvailSpaceList(catObjForFile, &pid, apage);
        if (e < 0) ERRB1(e, &pid, PAGE_BUF);

        if (SP_CFREE(apage) < neededSpace) { 
            e = EduOM_CompactPage(apage, -1);
            if (e < 0) ERR(e);
        }
    } else {
        e = BfM_FreeTrain(&pid, 0);
        if (e < 0) ERR(e);

        e = RDsM_PageIdToExtNo((PageID*)&pFid, &firstExt);
        if (e < 0) ERR(e);

        if (nearObj != NULL) {
            nearPid.pageNo = nearObj->pageNo;
            nearPid.volNo = nearObj->volNo;
        } else {
            nearPid.pageNo = catEntry->lastPage;
            nearPid.volNo = catEntry->fid.volNo;
        }

        e = RDsM_AllocTrains(catEntry->fid.volNo, firstExt, &nearPid, catEntry->eff, 1, PAGESIZE2, &pid);
        if (e < 0) ERR(e);

        e = BfM_GetNewTrain(&pid, (char **)&apage, PAGE_BUF);
        if (e < 0) ERR(e);

        apage->header.fid = fid;
        apage->header.pid = pid;
        apage->header.prevPage = NIL;
        apage->header.nextPage = NIL;
        apage->header.spaceListPrev = NIL;
        apage->header.spaceListNext = NIL;
        apage->header.nSlots = 1;
        apage->header.unused = 0;
        apage->header.unique = 0;
        apage->header.uniqueLimit = 0;
        apage->header.free = 0;
        apage->slot[0].offset = EMPTYSLOT;
        SET_PAGE_TYPE(apage, SLOTTED_PAGE_TYPE);

        e = om_FileMapAddPage(catObjForFile, (PageID*)nearObj, &pid);
        if (e < 0) ERRB1(e, &pid, PAGE_BUF);
    }

    obj = &apage->data[apage->header.free];
    memcpy(obj->data, data, length);
    
    obj->header = *objHdr;
    obj->header.length = length;

    for (i = 0; i < apage->header.nSlots; i++){
        if (apage->slot[-i].offset == EMPTYSLOT) {
            break;
        }
    }
    if (i == apage->header.nSlots) {
        apage->header.nSlots += 1;
    }

    apage->slot[-i].offset = apage->header.free;

    e = om_GetUnique(&pid, &apage->slot[-i].unique);
    if (e < 0) ERR(e);

    apage->header.free += sizeof(ObjectHdr) + alignedLen;

    oid->slotNo = i;
    oid->unique = apage->slot[-i].unique;
    oid->pageNo = pid.pageNo;
    oid->volNo = pid.volNo;

    e = BfM_SetDirty(&pid, PAGE_BUF);
    if (e < 0) ERRB1(e, &pid, PAGE_BUF);

    e = om_PutInAvailSpaceList(catObjForFile, &pid, apage);
    if (e < 0) ERRB1(e, &pid, PAGE_BUF);

    e = BfM_FreeTrain((TrainID*)catObjForFile, PAGE_BUF);
    if( e < 0 ) ERR( e );

    e = BfM_FreeTrain(&pid, PAGE_BUF);
    if( e < 0 ) ERR( e );

    return(eNOERROR);
} /* eduom_CreateObject() */

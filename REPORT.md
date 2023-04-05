# EduOM Report

Name: Nguyen Duong Tung

Student id: 49004034

# Problem Analysis

We implement the operations of the slotted page related structure to store objects. Which include implementing these functions:
EduOM_CreateObject()
EduOM_DestroyObject()
EduOM_CompactPage()
EduOM_ReadObject()
EduOM_NextObject()
EduOM_PrevObject()
And 1 internal function: eduom_CreateObject()

# Design For Problem Solving

## High Level

### eduom_CreateObject
Implements input validation, buffer management, and object creation within the data file. Finds or allocates a suitable page, creates a new object, and updates the page and slot information.

### EduOM_CreateObject
Implements a wrapper function that initializes the object header and calls the lower-level eduom_CreateObject function to create a new object in the data file.

### EduOM_DestroyObject
Implements a function to delete an object from the data file, update related page and slot information, and deallocate pages if necessary.

### EduOM_CompactPage
Implements a function to compact a slotted page by moving its objects and updating the slot offsets and page header information.

### EduOM_ReadOcject
Implements a function to read data from an object, handling input validation and buffer management, and returning the number of bytes read.

### EduOM_NextObject
Implements a function to find the next object in a data file, iterating through pages and slots in the forward direction and returning the ObjectID and header of the next object.

### EduOM_PrevObject
Implements a function to find the previous object in a data file, iterating through pages and slots in the reverse direction and returning the ObjectID and header of the previous object.

## Low Level

### eduom_CreateObject

Perform parameter checking and error handling.
Retrieve the buffer containing the catalog and the catalog entry.
Calculate the space needed to store the new object.
Select the appropriate page in which to insert the object, considering the nearObj and available space in the catalog entry.
If there is enough space in the selected page, remove it from the available space list and compact the page if necessary. Otherwise, allocate a new page, initialize it, and add it to the file map.
Create a new object within the selected page by copying the input data and setting the object header.
Find an empty slot for the new object, or create a new slot if necessary.
Update the slot information with the new object's offset and unique ID.
Update the page header with the new free space information.
Set the output ObjectID with the slot number, unique ID, page number, and volume number.
Mark the selected page as dirty and add it back to the available space list.
Free the buffer trains for the catalog object and the selected page.
Return a success status, indicating no errors.

### EduOM_CreateObject

Define a function EduOM_CreateObject that takes the following inputs:
catObjForFile: file in which the object is to be placed
nearObj: create the new object near this object
objHdr: object header from which tag is to be set
length: amount of data
data: the initial data for the object
oid: the object's ObjectID (output)
Perform parameter checking and error handling.
Create an ObjectHdr named objectHdr and initialize it with zeros.
If the input objHdr is not null, set the objectHdr.tag with the input objHdr->tag.
Call the function eduom_CreateObject with the following parameters:
catObjForFile
nearObj
&objectHdr
length
data
oid
If an error occurs during the call, return the error.
Return a success status, indicating no errors.

### EduOM_DestroyObject

Define a function EduOM_DestroyObject that takes the following inputs:
catObjForFile: file containing the object
oid: object to destroy
dlPool: pool of dealloc list elements
dlHead: head of dealloc list
Perform parameter checking and error handling.
Get the catalog object, file ID, and page ID associated with the object.
Retrieve the buffer holding the page containing the object.
Remove the page from the 'availSpaceList'.
Remove the object from the page.
Update the control information for 'unused', 'freeStart', and 'slot offset'.
If there are no more objects in the page and the page is not the first page in the file:
a. Remove the page from the filemap list.
b. Deallocate the page.
c. Add the deallocated page to the dealloc list.
Otherwise, if the page still has objects:
a. Set the page as dirty in the buffer manager.
b. Put the page back into the 'availSpaceList'.
Free the buffer holding the page.
Return a success status, indicating no errors.

### EduOM_CompactPage

Define a function EduOM_CompactPage that takes the following inputs:
apage: slotted page to compact
slotNo: slot number to move to the end
Create a temporary page (tpage) and copy the input page (apage) to it.
Initialize apageDataOffset to 0, which will be used to track the next object move position in the data area.
Loop through each non-empty slot in the temporary page tpage except the specified slotNo:
a. Calculate the length of the object, including the ObjectHdr.
b. Update the offset of the corresponding slot in the original page apage.
c. Copy the object from the temporary page to the original page at the updated offset.
d. Update the apageDataOffset to point to the next move position.
If the slotNo is not NIL:
a. Calculate the length of the object in the specified slotNo, including the ObjectHdr.
b. Copy the object from the temporary page to the original page at the current apageDataOffset.
c. Update the offset of the corresponding slot in the original page apage.
d. Update the apageDataOffset to point to the next move position.
Update the unused and free fields of the original page apage.
Return a success status, indicating no errors.

### EduOM_ReadObject

Define a function EduOM_ReadObject that takes the following inputs:
oid: object identifier to read
start: starting offset for reading
length: amount of data to read
buf: user buffer to store the read data
Check the validity of the input parameters:
a. If oid is NULL, return an error.
b. If length is negative and not equal to REMAINDER, return an error.
c. If buf is NULL, return an error.
Get the page containing the object specified by oid.
Check if the oid is valid for the given page.
Calculate the offset of the object in the page and get the object pointer obj.
If length is REMAINDER, update the length to be the difference between the object's total length and the start offset.
Copy the specified range (from start to length) of the object data to the user buffer buf.
Free the buffer page.
Return the number of bytes read.

### EduOM_NextObject

Define a function EduOM_NextObject that takes the following inputs:
catObjForFile: information about a data file
curOID: ObjectID of the current object
nextOID: ObjectID of the next object in the sequence
objHdr: the object header of the next object
Check the validity of the input parameters:
a. If catObjForFile is NULL, return an error.
b. If nextOID is NULL, return an error.
If curOID is not NULL, set the starting index i to the next slot after curOID and initialize pid with curOID's page and volume number.
Else, get the catalog entry and set i to 0, and initialize pid with the first page and volume number of the catalog entry.
Loop through the pages in the data file:
a. Get the page indicated by pid.
b. Loop through the slots in the page, starting from the index i:
i. If the slot contains a valid object, set the output values:
    1. Copy the object header to objHdr.
    2. Set the nextOID fields based on the current slot.
ii. Free the current page buffer and return.
c. Reset the index i to 0 and update the page number pid.pageNo to the next page number.
If no more pages are found, return End of Sequence (EOS).

### EduOM_PrevObject

Define a function EduOM_PrevObject that takes the following inputs:
catObjForFile: information about a data file
curOID: ObjectID of the current object
prevOID: ObjectID of the previous object in the sequence
objHdr: the object header of the previous object
Check the validity of the input parameters:
a. If catObjForFile is NULL, return an error.
b. If prevOID is NULL, return an error.
If curOID is not NULL, initialize pid with curOID's page and volume number.
Else, get the catalog entry and initialize pid with the last page and volume number of the catalog entry.
Loop through the pages in the data file in reverse order:
a. Get the page indicated by pid.
b. If curOID is not NULL, set the starting index i to the slot before curOID.
Else, set the index i to the last slot in the page.
c. Loop through the slots in the page in reverse order, starting from the index i:
i. If the slot contains a valid object, set the output values:
    1. Copy the object header to objHdr.
    2. Set the prevOID fields based on the current slot.
ii. Free the current page buffer and return.
d. Set curOID to NULL, update the page number pid.pageNo to the previous page number, and free the current page buffer.
If no more pages are found, return End of Sequence (EOS).

# Mapping Between Implementation And the Design

Code implementation is in the C file in the repository.
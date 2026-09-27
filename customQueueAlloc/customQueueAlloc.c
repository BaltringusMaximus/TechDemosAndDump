/*
INFO: 
total queue size INCLUDES the queue header size
when appending a packet to the queue, it checks if there is enough room in the queue by checking the total size minus the offset, the offset starts at sizeof(headerqueue)
splitBufferIntoSegments takes a big fat buffer and turns it into N queues if bQueue == 1, otherwise it'll simply turn it into N normal bufferinos

TO DO: make a mapping of queue ID - pointer

27092026 added queue id coordinate logic so that it looks more like this:
11 12 13 14 15 16 17 18 19 1A
21 22 23 24 ....
rather than merely incrementing by 1
*/


#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdbool.h>

#define sizeBuffer 20000
#define numberOfQueue 20

int intStop = 0;
int currentQueueId = 0x11;


typedef struct queueMapping
{
	int id;
	int queueOffset;
	int toBeRead;
	int checked;
	unsigned char* buffer;
	
	unsigned char* queueAddress;
}queueMapping;

typedef struct headerFreeMemory
{
	int isFree;
	int offset;
	int size;
}headerFreeMemory;

typedef struct headerPacket
{
	int isFree;
	int offset;
	int size;
}headerPacket;

typedef struct headerQueue
{
	int id;
	int totalSize;					//in bytes,
	int offset;						//where to copy da next element
	int numberOfElement;			//purely informative to make the struct 12 bytes long
}headerQueue;

typedef struct structPacket
{
	unsigned char* allocatedBufferAddress;
	unsigned char* packetData;
	int packetSize;
	bool FreeOrUsed;
}structPacket;


void bufferInit(unsigned char** buffer, int size);
void printBufferWithSize(unsigned char* buffer, int size);
void queueFromBuffer(unsigned char* buffer, unsigned char* queueAddress, int id);
void packetGenerator(int size, unsigned char** packet);
void packetAllocAndCopyToQueue(unsigned char* queue, structPacket* packet, int sizeOfPacket);
void splitBufferIntoSegments(unsigned char* buffer, int numberOfSegment, bool bQueue, unsigned char* queueMappingArray);
void printQueueMappingArray(unsigned char* queueMappingArray, int numOfQueue);
void printQueueMappingArrayReadable(unsigned char* queueMappingArray, int numOfQueue);
void queueCheck(int* id, int size, unsigned char* mappingArray, unsigned char* queue);
void retrieveQueueAddressFromId(int id, unsigned char* mappingArray, unsigned char* queue);
void readAndInitQueue(int id, unsigned char* mappingArray);
//void packetCopyToQueue(unsigned char* packet, unsigned char* queue);



void packetGenerator(int size, unsigned char** packet)
{
	unsigned char* tempPacket = NULL;
	printf("initial address = %p\n", *packet);
	*packet = malloc(size);

	printf("allocated address = %p\n", *packet);

	for (int i = 0; i < size; i++)
	{
		*(*packet+i)  = i;
		printf("%llX ", *(*packet + i));

	}

	return;
}

void bufferInit(unsigned char** buffer, int size)
{

	*buffer = malloc(sizeBuffer * sizeof(unsigned char) + sizeof(int));

	struct headerFreeMemory headerInit;
	headerInit.isFree = 1;
	headerInit.offset = 0;
	headerInit.size = size;
	memset(*buffer, 0, size);
	memcpy(*buffer, &headerInit, sizeof(struct headerFreeMemory));
//	memset(&(*buffer)[size], 0xFFFFFFFF, sizeof(int));
	memset((*buffer + size), 0xFFFFFFFF, sizeof(int));
	return;
}

void queueFromBuffer(unsigned char* buffer, unsigned char** queueAddress, int id)
{
	headerQueue headerAllocatedQueue;
	headerFreeMemory headerBuffer;
	memcpy(&headerBuffer, buffer, sizeof(headerFreeMemory));
	memset(&headerAllocatedQueue, 0, sizeof(headerQueue));
	printf("%d %d %d\n", headerBuffer.isFree, headerBuffer.offset, headerBuffer.size);
	headerAllocatedQueue.id = id;
	headerAllocatedQueue.numberOfElement = 0;
	headerAllocatedQueue.offset = sizeof(headerQueue);
	headerAllocatedQueue.totalSize = headerBuffer.size;

	memcpy(buffer, &headerAllocatedQueue, sizeof(headerQueue));
	if (queueAddress != NULL){ *queueAddress = buffer; }
	else { printf("no queueAddress pointer provided\n"); }
//	*queueAddress = buffer;
	return;
}
void printBufferWithSize(unsigned char* buffer, int size)
{
	printf("\n");
	for (int i = 0; i < size + sizeof(int); i++)
	{
		printf("%d ", buffer[i]);
	}
	printf("\n");
	return;
}

void packetAllocAndCopyToQueue(unsigned char* queue, structPacket* packet, int sizeOfPacket)
{
	headerQueue *currentQueue = queue;
	headerPacket headerCurrentPacket;
	headerFreeMemory headerFollowingMemory;
	bool addingPadding = 0;
	if (sizeOfPacket + sizeof(headerPacket) > (currentQueue->totalSize - currentQueue->offset)) { printf("not enough room to append packet to the queue\n"); return; }
	if (sizeOfPacket + sizeof(headerPacket) + sizeof(headerFreeMemory) > (currentQueue->totalSize - currentQueue->offset)) { printf("not adding headerFreeMemory, adding padding instead\n "); addingPadding = 1; }
	printf("queue details = %d %d %d\n", currentQueue->numberOfElement, currentQueue->offset, currentQueue->totalSize);
	packet->packetSize = sizeOfPacket;
	packetGenerator(packet->packetSize, &packet->packetData);
	packet->allocatedBufferAddress = queue + currentQueue->offset;
	printf("allocated address at = %p\n", packet->allocatedBufferAddress);



	if (addingPadding == 0)
	{
		headerCurrentPacket.isFree = 0;
		headerCurrentPacket.size = sizeOfPacket;
		headerCurrentPacket.offset = currentQueue->offset;

		currentQueue->offset += (sizeOfPacket + sizeof(headerPacket));
		currentQueue->numberOfElement++;

		headerFollowingMemory.isFree = 1;
		headerFollowingMemory.size = currentQueue->totalSize - currentQueue->offset;
		headerFollowingMemory.offset = currentQueue->offset;
		memcpy(packet->allocatedBufferAddress, &headerCurrentPacket, sizeof(headerPacket));
		memcpy(packet->allocatedBufferAddress + sizeof(headerPacket), packet->packetData, packet->packetSize);
		memcpy(packet->allocatedBufferAddress + sizeof(headerPacket) + sizeOfPacket, &headerFollowingMemory, sizeof(headerFreeMemory));
	}


	else if (addingPadding == 1)
	{
		headerCurrentPacket.isFree = 0;
		headerCurrentPacket.size = currentQueue->totalSize - currentQueue->offset - sizeof(headerPacket);
		headerCurrentPacket.offset = currentQueue->offset;

		currentQueue->offset += (headerCurrentPacket.size + sizeof(headerPacket));
		currentQueue->numberOfElement++;

		memcpy(packet->allocatedBufferAddress, &headerCurrentPacket, sizeof(headerPacket));
		memcpy(packet->allocatedBufferAddress + sizeof(headerPacket), packet->packetData, packet->packetSize);
		memset(packet->allocatedBufferAddress + sizeof(headerPacket) + sizeOfPacket, (unsigned char)0xF, headerCurrentPacket.size - sizeOfPacket);
	}
	
	//if (address == NULL) { printf("alloc failed, no address found, couldn't copy the packet\n"); return; }
	packet->FreeOrUsed = 1;
	return;
}

// must be preinitialized buffer USE BEFORE TURNING IT INTO A FUCKING QUEUE

void splitBufferIntoSegments(unsigned char* buffer, int numberOfSegment, bool bQueue, unsigned char* queueMappingArray)
{
	int id = 0x11;
	headerFreeMemory headerCurrentSegment;
	queueMapping* currentQueueMapping = queueMappingArray;
	printf("currentQueueMapping = %p queueMappingArray = %p\n", currentQueueMapping, queueMappingArray);
	memcpy(&headerCurrentSegment, buffer, sizeof(headerCurrentSegment));
	int totalSize = headerCurrentSegment.size;
	if (totalSize % numberOfSegment != 0) { printf("can't split da buffer into %d segments cuz the requested number of segments doesn't divide the total size of the buffer supplied\n", numberOfSegment); return; }
	for (int i = 0; i < numberOfSegment; i++)
	{
		headerCurrentSegment.isFree = 1;
		headerCurrentSegment.offset = ( i * totalSize / numberOfSegment );
		headerCurrentSegment.size = totalSize / numberOfSegment;
		printf("%d %d %d\n", headerCurrentSegment.isFree, headerCurrentSegment.offset, headerCurrentSegment.size);
		printf("%p\n", buffer + headerCurrentSegment.offset);
//		printBufferWithSize(buffer, sizeBuffer);
		memcpy((buffer + headerCurrentSegment.offset), &headerCurrentSegment, sizeof(headerFreeMemory));
		//if (i % 10 == 0) { id = id + 0x10; }
		if (i != 0) { id = id + (int)(0x10 * ((i - i % 10) / i)) + (int)(((i % 10 - (i - 1) % 10))); }
		
		if (bQueue == 1) { queueFromBuffer(buffer + headerCurrentSegment.offset, NULL, id); }
		if (queueMappingArray != NULL) 
		{ 
			currentQueueMapping->id = id; 
			currentQueueMapping->buffer = buffer;
			currentQueueMapping->queueAddress = buffer + headerCurrentSegment.offset;
			currentQueueMapping->queueOffset = headerCurrentSegment.offset;
			currentQueueMapping = currentQueueMapping + 1; // understand "+ 1*sizeof(queueMapping)" basically, what an odd behaviour
			printf("currentQueueMapping = %p\n", currentQueueMapping); 
		}
		
		printf("exiting the split function\n");
//		printBufferWithSize(buffer, sizeBuffer);
	}
	
	return;
}

void printQueueMappingArray(unsigned char* queueMappingArray, int numOfQueue)
{
	
	for (int j = 0; j < numOfQueue; j++)
	{
		for (int i = 0; i < sizeof(queueMapping); i++)
		{
			printf("%d ", queueMappingArray[i]);
		}
		printf("\n");
		queueMappingArray += sizeof(queueMapping);
	}

	return;
}

void printQueueMappingArrayReadable(unsigned char* queueMappingArray, int numOfQueue)
{
	queueMapping* currentQueue = queueMappingArray;
	printf("list of all registered Queues\n");
	for (int j = 0; j < numOfQueue; j++)
	{
		printf("queue ID = %X\nqueue offset = %d\nqueue address = %p\nbuffer address = %p\nPending to be read? = %d\nhas been checked? = %d\n", currentQueue->id, currentQueue->queueOffset, currentQueue->queueAddress, currentQueue->buffer,currentQueue->toBeRead,currentQueue->checked);
		currentQueue += 1;
		printf("\n");
		queueMappingArray += sizeof(queueMapping);
	}

	return;
}

void queueCheck(int* id, int size, unsigned char* mappingArray, unsigned char* queue)
{
	
	printf("id = %X\n", *id);
	queueMapping* queueCheckCleanUp = (queueMapping*)mappingArray;
	queueMapping* currentQueueMapped = (queueMapping*)(mappingArray + (*id-0x11)*sizeof(queueMapping));
	headerQueue* currentQueue = (headerQueue*)currentQueueMapped->queueAddress;
	printf("queue stats: %X %d %d %d\n", currentQueue->id, currentQueue->numberOfElement, currentQueue->offset, currentQueue->totalSize);
	if (size + sizeof(headerPacket) > (currentQueue->totalSize - currentQueue->offset) || currentQueueMapped->checked == 1/* && (*id) < (0x11 + numberOfQueue - 1) */ )
	{ 
		currentQueueMapped->checked = 1;
		printf("not enough room to append packet to the queue\n");
		currentQueueMapped->toBeRead = 1;
//		if ((*id + 0x1) >= (0x11 + numberOfQueue)) { printf("no queue shall bear your load :^3\n"); return; }
		if ((*id + 0x1) >= (0x11 + numberOfQueue)) { *id = 0x11; printf("returning to beginning of mapping\n"); }
		else { *id = (currentQueueMapped + 1)->id; }
		if ((currentQueueMapped + 1)->checked == 1) { printf("full loop completed, no queue shall bear your load :^3\n"); return; }
		printf("new id = %X\n", *id);
		queueCheck(id,size,mappingArray,queue);
		
	}
	printf("queue %X selected\n", *id);
	printQueueMappingArray(mappingArray, numberOfQueue);
	for (int i = 0; i < numberOfQueue; i++)
	{
		printf("%p %p %d %d %d %d\n", queueCheckCleanUp->buffer, queueCheckCleanUp->queueAddress, queueCheckCleanUp->checked, queueCheckCleanUp->id, queueCheckCleanUp->queueOffset, queueCheckCleanUp->toBeRead);
		queueCheckCleanUp->checked = 0;
		queueCheckCleanUp += 1;
	}
	printQueueMappingArray(mappingArray, numberOfQueue);
	return;
}

void retrieveQueueAddressFromId(int id, unsigned char* mappingArray, unsigned char** queue)
{
	queueMapping* currentQueueMapped = (queueMapping*)(mappingArray + (id-0x11) * sizeof(queueMapping));
	*queue = currentQueueMapped->queueAddress;
	printf("queue %X address %p\n", id, *queue);
	return;
}

void readAndInitQueue(int id, unsigned char* mappingArray)
{
	unsigned char* queue;
	retrieveQueueAddressFromId(id, mappingArray, &queue);
	headerQueue* currentQueue = (headerQueue*)queue;
	queueMapping* currentQueueMapped = (queueMapping*)(mappingArray + (currentQueue->id-0x11) * sizeof(queueMapping));
	printf("readAndInit queue stats = ID %d #elts %d offset %d total size %d\n", currentQueue->id, currentQueue->numberOfElement, currentQueue->offset, currentQueue->totalSize);
	memset(queue + sizeof(headerQueue), 0, currentQueue->totalSize - sizeof(headerQueue));
	currentQueue->numberOfElement = 0;
	currentQueue->offset = sizeof(headerQueue);
	currentQueueMapped->toBeRead = 0;
	printf("readAndInit queue stats = ID %d #elts %d offset %d total size %d\n", currentQueue->id, currentQueue->numberOfElement, currentQueue->offset, currentQueue->totalSize);

	return;
}

int main()
{

	unsigned char* pBufferByte = NULL;
	unsigned char* pQueueByte = NULL;
	int idQueue1 = 0x11;
	int idQueue2 = 0x12;
	unsigned char* queueMappingArray = NULL;
	structPacket packet1;
	structPacket packet2;
	int sizePacket = 0;
	int queueToPurge = 0;
	memset(&packet1, 0, sizeof(structPacket));

	queueMappingArray = malloc(numberOfQueue * sizeof(queueMapping));
	memset(queueMappingArray, 0, numberOfQueue * sizeof(queueMapping));
	printf("size of queuemappingarray = %d\n", numberOfQueue * sizeof(queueMapping));
	
	printQueueMappingArray(queueMappingArray, numberOfQueue);
	printf("Buffer byte  %p\n", pBufferByte);
	bufferInit(&pBufferByte, sizeBuffer);
	printf("Buffer byte  %p\n", pBufferByte);
	printBufferWithSize(pBufferByte, sizeBuffer);
	splitBufferIntoSegments(pBufferByte, numberOfQueue, 1, queueMappingArray);
	printf("back to main\n");
	printQueueMappingArray(queueMappingArray, numberOfQueue);
	printQueueMappingArrayReadable(queueMappingArray, numberOfQueue);
	printBufferWithSize(pBufferByte, sizeBuffer);
	printf("back to main again\n");
	//packetGenerator(11, &packet1.packetData);
	printf("\n");
	while (intStop == 0)
	{
		printf("input packet size\n");
		scanf_s("%d", &sizePacket);
		queueCheck(&currentQueueId, sizePacket, queueMappingArray, NULL);
		printQueueMappingArrayReadable(queueMappingArray, numberOfQueue);
		retrieveQueueAddressFromId(currentQueueId, queueMappingArray, &pQueueByte);

		printf("currentQueueId = %X\n", currentQueueId);
		printf("current queue address = %p\n", pQueueByte);
		packetAllocAndCopyToQueue(pQueueByte, &packet1, sizePacket);
		printBufferWithSize(pBufferByte, sizeBuffer);
		printf("continue?\n");
		scanf_s("%d", &intStop);
		printf("type in the queue to purge or 0 if u don't wanna purge any queue\nQueue list = \n");
		for (int i = 0; i < numberOfQueue; i++)
		{
			printf("%d ", 0x11 + i);
		}
		printf("\n");
		scanf_s("%d", &queueToPurge);
		while (queueToPurge != 0)
		{

			while ((queueToPurge < 0x11 && queueToPurge != 0) || queueToPurge > 0x11 + numberOfQueue - 1)
			{
				printf("my brother in Christ, da queue u selected doesn't exist\n");
				scanf_s("%d", &queueToPurge);
			}
			if (queueToPurge == 0) { break; }
			printf("queue %d to be purged\n");
			readAndInitQueue(queueToPurge, queueMappingArray);
			printQueueMappingArray(queueMappingArray, numberOfQueue);
			printBufferWithSize(pBufferByte, sizeBuffer);
			printf("exit 0 or type in another queue?\n ");
			scanf_s("%d", &queueToPurge);
			printf("queue to purge = %d\n", queueToPurge);

			
		}
		
		

	}
	/*
	readAndInitQueue(0x11,queueMappingArray);
	printQueueMappingArray(queueMappingArray, numberOfQueue);
	printBufferWithSize(pBufferByte, sizeBuffer);
	readAndInitQueue(0x12, queueMappingArray);
	printQueueMappingArray(queueMappingArray, numberOfQueue);
	printBufferWithSize(pBufferByte, sizeBuffer);
	readAndInitQueue(0x13, queueMappingArray);
	printQueueMappingArray(queueMappingArray, numberOfQueue);
	printBufferWithSize(pBufferByte, sizeBuffer);
	*/
	/*
	queueCheck(&currentQueueId, 10, queueMappingArray, NULL);
	queueMapping* currentQueueMapped = (queueMapping*)(queueMappingArray + 0*sizeof(queueMapping));
	printf("queueMappingArray = %X, %d, %p, %p\n", currentQueueMapped->id, currentQueueMapped->queueOffset, currentQueueMapped->buffer, currentQueueMapped->queueAddress);
	packetAllocAndCopyToQueue(currentQueueMapped->queueAddress, &packet1, 10);
	printBufferWithSize(pBufferByte, sizeBuffer);
	packetAllocAndCopyToQueue(currentQueueMapped->queueAddress, &packet2, 938);
//	printBufferWithSize(pBufferByte, sizeBuffer);
	*/
	/*
	queueCheck(&currentQueueId, 10, queueMappingArray, NULL);
	retrieveQueueAddressFromId(currentQueueId, queueMappingArray, &pQueueByte);
	
	printf("currentQueueId = %X\n", currentQueueId);
	printf("current queue address = %p\n", pQueueByte);
	packetAllocAndCopyToQueue(pQueueByte, &packet1, 10);
	printBufferWithSize(pBufferByte, sizeBuffer);
	
	queueCheck(&currentQueueId, 10, queueMappingArray, NULL);
	retrieveQueueAddressFromId(currentQueueId, queueMappingArray, &pQueueByte);

	printf("currentQueueId = %X\n", currentQueueId);
	printf("current queue address = %p\n", pQueueByte);
	packetAllocAndCopyToQueue(pQueueByte, &packet1, 10);
	printBufferWithSize(pBufferByte, sizeBuffer);
	queueCheck(&currentQueueId, 10, queueMappingArray, NULL);
	retrieveQueueAddressFromId(currentQueueId, queueMappingArray, &pQueueByte);

	printf("currentQueueId = %X\n", currentQueueId);
	printf("current queue address = %p\n", pQueueByte);
	packetAllocAndCopyToQueue(pQueueByte, &packet1, 10);
	printBufferWithSize(pBufferByte, sizeBuffer);
	queueCheck(&currentQueueId, 10, queueMappingArray, NULL);
	retrieveQueueAddressFromId(currentQueueId, queueMappingArray, &pQueueByte);

	printf("currentQueueId = %X\n", currentQueueId);
	printf("current queue address = %p\n", pQueueByte);
	packetAllocAndCopyToQueue(pQueueByte, &packet1, 10);
	printBufferWithSize(pBufferByte, sizeBuffer);
	queueCheck(&currentQueueId, 900, queueMappingArray, NULL);
	retrieveQueueAddressFromId(currentQueueId, queueMappingArray, &pQueueByte);

	printf("currentQueueId = %X\n", currentQueueId);
	printf("current queue address = %p\n", pQueueByte);
	packetAllocAndCopyToQueue(pQueueByte, &packet1, 900);
	printBufferWithSize(pBufferByte, sizeBuffer);
	queueCheck(&currentQueueId, 400, queueMappingArray, NULL);
	retrieveQueueAddressFromId(currentQueueId, queueMappingArray, &pQueueByte);

	printf("currentQueueId = %X\n", currentQueueId);
	printf("current queue address = %p\n", pQueueByte);
	packetAllocAndCopyToQueue(pQueueByte, &packet1, 400);
	printBufferWithSize(pBufferByte, sizeBuffer);
	queueCheck(&currentQueueId, 900, queueMappingArray, NULL);
	retrieveQueueAddressFromId(currentQueueId, queueMappingArray, &pQueueByte);

	printf("currentQueueId = %X\n", currentQueueId);
	printf("current queue address = %p\n", pQueueByte);
	packetAllocAndCopyToQueue(pQueueByte, &packet1, 900);
	printBufferWithSize(pBufferByte, sizeBuffer);
	*/

	/*
	queueFromBuffer(pBufferByte, &pQueueByte,idQueue1);
	printf("queue address = %p\n", pQueueByte);
	printBufferWithSize(pBufferByte, sizeBuffer);
	packetAllocAndCopyToQueue(pQueueByte, &packet1, 10);
	printBufferWithSize(pBufferByte, sizeBuffer);
	packetAllocAndCopyToQueue(pQueueByte, &packet2, 943);
	printBufferWithSize(pBufferByte, sizeBuffer);
	//packetGenerator(10, &pPacket1);
	*/
	return 0;
}

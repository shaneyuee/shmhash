/******************************************************************************

 FILENAME:	link_table.cc

  DESCRIPTION:	����ʽ�ڴ�ṹʵ��

  �������ȡ�����ݴ洢��̬���鲿�֣��ڴ�����������֯���ϲ����
  ��ο���
      http://km.oa.com/group/578/articles/show/125202
      http://km.oa.com/group/17746/docs/show/112075

 HISTORY:
           Date          Author         Comment
        2013-11-12      Shaneyu        Created based on UDC's LinkTable
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <iostream>

#include "link_table.h"

// if you have Attr_API, please define HAS_ATTR_API in your makefile
// if you don't have one, we won't blame it, you can download it at http://www.server.com/oss
#ifdef HAS_ATTR_API
#include "Attr_API.h"
#else
#define Attr_API(a, b)
#endif

//��ʼ����ʽ���û������Լ�����LinkTableHead��Block�����buffer
//���Ե���EvalSize����Block��Ҫ��buffer��С
int LinkTable::Init(void *pBuffer, ull64_t ddwBufferSize, ull64_t ddwBlockCount, ull64_t ddwBlockSize)
{
	if(pBuffer==NULL || ddwBufferSize<EvalBufferSize(ddwBlockCount, ddwBlockSize))
	{
		snprintf(errmsg, sizeof(errmsg), "Bad argument: %s",
			pBuffer==NULL? "buffer is null": "not enough buffer size");
		return -1;
	}

	header = (volatile LinkTableHead *)pBuffer;
	blocks = ((char*)pBuffer)+sizeof(LinkTableHead);

	memset((void*)header, 0, sizeof(*header));
	header->ddwAllBlockCount = header->ddwFreeBlockCount = (ddwBlockCount - 1);
	header->ddwBlockSize = ddwBlockSize;
	header->ddwLastEmpty = 1; // the first block (pos==0) is not used
	header->dwRecyclePoolSize = LT_DEF_RECYCLE_POOL_SIZE;

	return 0;
}

int LinkTable::InitExisting(void *pBuffer, ull64_t ddwBufferSize, ull64_t ddwBlockCount, ull64_t ddwBlockSize)
{
	if(pBuffer==NULL || ddwBufferSize<EvalBufferSize(ddwBlockCount, ddwBlockSize))
	{
		snprintf(errmsg, sizeof(errmsg), "Bad argument: %s",
			pBuffer==NULL? "buffer is null": "not enough buffer size");
		return -1;
	}

	header = (volatile LinkTableHead *)pBuffer;
	blocks = ((char*)pBuffer)+sizeof(LinkTableHead);

	if(header->ddwAllBlockCount != (ddwBlockCount - 1))
	{
		snprintf(errmsg, sizeof(errmsg), "Total block count mismatched: existing %llu, expected %llu", header->ddwAllBlockCount, (ddwBlockCount - 1));
		return -2;
	}

	if(header->ddwBlockSize != ddwBlockSize)
	{
		snprintf(errmsg, sizeof(errmsg), "Block size mismatched: existing %llu, expected %llu", header->ddwBlockSize, ddwBlockSize);
		return -3;
	}

	return 0;
}


#define _GET_ELE_AT(elist, idx, sz) ((LtBlock *)(((char *)(elist)) + ((idx) * (sz))))

// ����������ָ���Block
#define LT_ELE_AT(idx) _GET_ELE_AT(blocks, idx, header->ddwBlockSize)

// �ͷ������Ԫ�ز��ŵ�free������
int LinkTable::FreeBlock(ull64_t ddwStartPos)
{
	ull64_t ddwCurPos=0, ddwNextPos=0;
	int iCount=0;
	LtBlock *pBlock;

	if(header==NULL || blocks==NULL)
	{
		strncpy(errmsg, "LinkTable not initialized", sizeof(errmsg));
		return -1;
	}

	if((ddwStartPos==0) || (ddwStartPos>=header->ddwAllBlockCount))
	{
		Attr_API(355929 , 1); // Position gone insane
		snprintf(errmsg, sizeof(errmsg), "Position %llu gone insane", ddwStartPos);
		return -2;
	}

	ddwNextPos=ddwStartPos;
	while((ddwNextPos) &&(iCount<=MAX_BLOCK_COUNT))
	{
		ddwCurPos=ddwNextPos;
		if(ddwCurPos>=header->ddwAllBlockCount)
		{
			Attr_API(355929, 1); // Position gone insane
			snprintf(errmsg, sizeof(errmsg), "Position %llu gone insane", ddwStartPos);
			return -3;
		}

		pBlock=LT_ELE_AT(ddwCurPos);
		ddwNextPos=pBlock->ddwNext;
		memset(pBlock, 0, header->ddwBlockSize);

		// �ѽڵ����Free�б���
		pBlock->ddwNext=header->ddwFirstFreePos;
		header->ddwFirstFreePos=ddwCurPos;

		header->ddwFreeBlockCount++;
		iCount++;
	}

	if(ddwNextPos!=0)
	{
		Attr_API(355931 , 1); // Too many blocks
		snprintf(errmsg, sizeof(errmsg), "Too many blocks at %llu", ddwStartPos);
		return -4;
	}

	return 0;
}


// Ԥ���ճص���Ҫ�㷨ʵ�֣�
// ��һ���α�dwRecycleIndexѭ������Ԥ���ճ�ָ������addwRecyclePool��û��Ԥ���ձ���һ��ָ�룬
// ������ϣ�����յ���ʼ��ַ�ҵ�addwRecyclePool[ddwRecycleIndex]�ϣ��������ϴιҵ���ĵط���Ԫ������
int LinkTable::RecycleBlock(ull64_t ddwStartPos)
{
	int iRet;
	ull64_t ddwOldPos, ddwIdx;

	if(header==NULL || blocks==NULL)
	{
		strncpy(errmsg, "LinkTable not initialized", sizeof(errmsg));
		return -1;
	}

	ddwIdx = header->ddwRecycleIndex;
	if(ddwIdx>=header->dwRecyclePoolSize)
	{
		Attr_API(355950, 1);
		ddwIdx = 0;
		header->ddwRecycleIndex = 0;
	}

	// �ȷŵ�Ԥ���ճ��У��ٻ���ͬ��λ���еľ�����
	ddwOldPos = header->addwRecyclePool[ddwIdx];
	header->addwRecyclePool[ddwIdx] = ddwStartPos;
	header->ddwRecycleIndex = (ddwIdx+1)%header->dwRecyclePoolSize;

	if(ddwOldPos)
	{
		iRet = FreeBlock(ddwOldPos);
		if(iRet<0)
		{
			// �������ʧ�ܣ������µ�Ԫ���ܱ��������ã�����ֻ�ϱ���������
			Attr_API(355951, 1);
		}
	}

	return 0;
}

// ����ȫ��Ԥ���ճ��е�Ԫ��
void LinkTable::EmptyRecyclePool()
{
	int i;

	if(header==NULL || blocks==NULL)
		return;

	for(i=0; i<(int)header->dwRecyclePoolSize; i++)
	{
		if(header->addwRecyclePool[i])
		{
			int iRet;

			iRet = FreeBlock(header->addwRecyclePool[i]);
			if(iRet<0)
			{
				// �������ʧ�ܣ������µ�Ԫ���ܱ��������ã�����ֻ�ϱ���������
				Attr_API(355951, 1);
			}

			header->addwRecyclePool[i] = 0;
		}
	}

	header->ddwRecycleIndex = 0;
}

// ---------- ����� ATTR_API ������Ҫ��������

//ʹ�����鷽ʽ�������ɿռ䣬�����ʹ�����ɿռ���ò�����ʽ
// ���� -100 ��ʾ�ռ䲻��
int LinkTable::GetEmptyBlock(int iCount, ull64_t &ddwOutPos)
{
	ull64_t  ddwStartPos,ddwCurPos;
	int i,j;
#ifdef EMPTY_RECYCLE_ON_OUT_OF_SPACE
	int iTrySecond=0;
#endif
	float fUsedRate;
	LtBlock *pBlock;

	if(header==NULL || blocks==NULL)
	{
		strncpy(errmsg, "LinkTable not initialized", sizeof(errmsg));
		return -1;
	}

	// ֻҪ���п���С��Ԥ������͸澯
	fUsedRate = (header->ddwFreeBlockCount*100.0/header->ddwAllBlockCount);
	if(fUsedRate<20.0) Attr_API(355995, 1);
	if(fUsedRate<15.0) Attr_API(355996, 1);
	if(fUsedRate<10.0) Attr_API(355997, 1);
	if(fUsedRate<5.0)  Attr_API(355998, 1);

#ifdef EMPTY_RECYCLE_ON_OUT_OF_SPACE
restart:
	if(iTrySecond) // ���Ԥ���ճغ����·���
		Attr_API(355999, 1);
#endif
	if((iCount<0) ||
		(iCount > MAX_BLOCK_COUNT) ||
		(iCount > (int)header->ddwFreeBlockCount))
	{
		// Not enough space
#ifdef EMPTY_RECYCLE_ON_OUT_OF_SPACE
		if(iTrySecond==0)
		{
			iTrySecond = 1;
			EmptyRecyclePool();
			goto restart;
		}
#endif
		Attr_API(356005, 1);
		strncpy(errmsg, "Not enough space", sizeof(errmsg));
		return -100;
	}

	// �ȴ�Free�б��������Ѳ����ٰ�������
	ddwStartPos=0;
	ddwCurPos=header->ddwFirstFreePos;
	i=0;
	while(i<iCount && ddwCurPos>0 &&
		  ddwCurPos<header->ddwAllBlockCount &&
		  (pBlock=LT_ELE_AT(ddwCurPos))->ddwLengthInBlock==0)
	{
		ull64_t ddwNext;
		i++;
		ddwNext = pBlock->ddwNext;
		if(i==iCount) // �����һ����Next����
			pBlock->ddwNext = 0;
		ddwCurPos=ddwNext;
	}

	if(i==iCount) // �ҵ���
	{
		ddwOutPos = header->ddwFirstFreePos;
		header->ddwFirstFreePos = ddwCurPos;
		return 0;
	}

	// FIXME���������һ�����Ǻ���Ҫ�����⣺���ǵ���Free�������ҵ�һЩ�ڵ㣬���޷������������Ҫ��
	// ��ʱ��Ҫͨ���������������䣬�������������ܻ��ҵ�free�����еĽڵ㣬�⽫����free�����еĽڵ㱻
	// ���������ˣ�ʹ��Free���������������������ֻ���ɱ�����������������

	// ��header->ddwLastEmpty��ʼ���а�������
	ddwStartPos=0;
	ddwCurPos=header->ddwLastEmpty;
	i=0;
	for(j=0; (i<iCount) && (j<(int)header->ddwAllBlockCount);j++)
	{
		if((pBlock=LT_ELE_AT(ddwCurPos))->ddwLengthInBlock==0)
		{
			pBlock->ddwNext=ddwStartPos;
			ddwStartPos=ddwCurPos;
			i++;
		}
		ddwCurPos++;

		// Wrap around
		if(ddwCurPos>=header->ddwAllBlockCount)
		{
			ddwCurPos=1;
		}
	}

	if(i<iCount)
	{
		// Not enough space
#ifdef EMPTY_RECYCLE_ON_OUT_OF_SPACE
		if(iTrySecond==0)
		{
			iTrySecond = 1;
			EmptyRecyclePool();
			goto restart;
		}
#endif
		Attr_API(356005, 1);
		strncpy(errmsg, "Not enough space", sizeof(errmsg));
		return -100;
	}

	ddwOutPos=ddwStartPos;
	header->ddwLastEmpty=ddwCurPos;
	return 0;
}

#define BLOCK_DATA_LEN() (header->ddwBlockSize - (unsigned long)(((LtBlock*)0)->bufData))

//����ddwPosָ����û�����
int LinkTable::GetData(ull64_t ddwPos, void *sDataBuf, int &iOutDataLen)
{
	ull64_t  ddwCurPos=0,ddwNextPos=0;
	int iDataLen=0,iBufLen=iOutDataLen;
	LtBlock *pBlock;

	if(sDataBuf==NULL ||
		iBufLen < (int)BLOCK_DATA_LEN()) // buffer��С����һ����
	{
		strncpy(errmsg, "Bad argument", sizeof(errmsg));
		return -1;
	}

	if(header==NULL || blocks==NULL)
	{
		strncpy(errmsg, "LinkTable not initialized", sizeof(errmsg));
		return -1;
	}

	iDataLen=0;

	ddwNextPos=ddwPos;
	while(ddwNextPos)
	{
		ddwCurPos=ddwNextPos;
		if(ddwCurPos>=header->ddwAllBlockCount)
		{
			Attr_API(355929, 1);
			strncpy(errmsg, "Position gone insane", sizeof(errmsg));
			return -21; // FIXME: HashTable::Get() will depend on this ret value to reclaim ht node, please dont change it!
		}
		if((iDataLen+(int)BLOCK_DATA_LEN()) > iBufLen)
		{
			Attr_API(356018, 1);
			strncpy(errmsg, "Not enough space for data", sizeof(errmsg));
			return -22; // FIXME: HashTable::Get() will depend on this ret value to reclaim ht node, please dont change it!
		}
		pBlock = LT_ELE_AT(ddwCurPos);
		uint16_t wBlockLen;
		if(pBlock->ddwNext) // not the last block
		{
			wBlockLen = BLOCK_DATA_LEN();
		}
		else // the last block
		{
			wBlockLen = pBlock->ddwLengthInBlock;
			if(wBlockLen>BLOCK_DATA_LEN())
				wBlockLen = BLOCK_DATA_LEN();
		}
		memcpy(((char*)sDataBuf)+iDataLen,pBlock->bufData, wBlockLen);
		iDataLen+=wBlockLen;
		ddwNextPos=pBlock->ddwNext;
	}
	iOutDataLen=iDataLen;
	return 0;
}


//�������ݣ����ddwPosָ���������Ч��ɾ����������
//ִ�гɹ�ʱ��ddwPos �����²��������λ��
// ���� -100 ��ʾ�ռ䲻��
int LinkTable::SetData(ull64_t &ddwPos, const void *sDataBuf, int iDataLen)
{
	ull64_t ddwCurPos=0,ddwNextPos=0,ddwStartPos=0,ddwOldPos=ddwPos;
	int iCount=0,iLeftLen=0,iCopyLen=0;
	int iRet=0;
	LtBlock *pBlock;

	if(sDataBuf==NULL || iDataLen<0)
	{
 		strncpy(errmsg, "Bad argument", sizeof(errmsg));
		return -1;
	}

	if(header==NULL || blocks==NULL)
	{
		strncpy(errmsg, "LinkTable not initialized", sizeof(errmsg));
		return -1;
	}

	iCount=(iDataLen+BLOCK_DATA_LEN()-1)/BLOCK_DATA_LEN();
	if(iCount>MAX_BLOCK_COUNT)
	{
		Attr_API(356019, 1);
 		snprintf(errmsg, sizeof(errmsg), "Data too large to fit in max of %d blocks", MAX_BLOCK_COUNT);
		return -2;
	}

	//�ȹ���������
	iRet=GetEmptyBlock(iCount, ddwStartPos);
	if(iRet<0)
	{
		if(iRet==-100)
			return iRet;
		return -7;
	}

	ddwNextPos=ddwStartPos;
	iLeftLen=iDataLen-iCopyLen;
	while((ddwNextPos) && (iLeftLen>0))
	{
		ddwCurPos=ddwNextPos;
		if(ddwCurPos>=header->ddwAllBlockCount)
		{
			Attr_API(355929, 1);
		 	strncpy(errmsg, "Next position is bad", sizeof(errmsg));
			return -8;
		}

		pBlock = LT_ELE_AT(ddwCurPos);
		if(iLeftLen > (int)BLOCK_DATA_LEN())
		{
			memcpy(pBlock->bufData,
				((char*)sDataBuf)+iCopyLen,BLOCK_DATA_LEN());
			iCopyLen+=BLOCK_DATA_LEN();
			pBlock->ddwLengthInBlock = BLOCK_DATA_LEN();
		}
		else
		{
			memcpy(pBlock->bufData,
					((char*)sDataBuf)+iCopyLen,(unsigned)iLeftLen);
			iCopyLen+=iLeftLen;
			pBlock->ddwLengthInBlock = iLeftLen;
		}

		ddwNextPos=pBlock->ddwNext;
		header->ddwFreeBlockCount--;
		iLeftLen=iDataLen-iCopyLen;
	}

	if(iLeftLen!=0)
	{
		//bug
		Attr_API(356083, 1);
		snprintf(errmsg, sizeof(errmsg),
			"Allocated blocks(%d) not enough for data(len=%d, bs=%llu)",
			iCount, iDataLen, header->ddwBlockSize);
		return -9;
	}

	LT_ELE_AT(ddwCurPos)->ddwNext=0;
	LT_ELE_AT(0)->ddwNext=ddwNextPos;
	ddwPos=ddwStartPos;

	//ɾ��������
	if(ddwOldPos!=0)
	{
		iRet=RecycleBlock(ddwOldPos);
		if(iRet<0)
		{
			// No way of turning back
			Attr_API(356091, 1);
		}
	}

	return 0;
}

//���ĳ��Key��Ӧ������
int LinkTable::EraseData(ull64_t ddwPos)
{
	if(header==NULL || blocks==NULL)
	{
		strncpy(errmsg, "LinkTable not initialized", sizeof(errmsg));
		return -1;
	}

	return RecycleBlock(ddwPos);
}

// �����û��ı�Ԥ���ճصĴ�С��sz������1��LT_MAX_PREFREE_POOL_SIZE֮��
int LinkTable::SetRecyclePoolSize(int sz)
{
	if(sz<1 || (unsigned int)sz>LT_MAX_RECYCLE_POOL_SIZE)
	{
		strncpy(errmsg, "Invalid argument", sizeof(errmsg));
		return -1;
	}
	if(header==NULL || blocks==NULL)
	{
		strncpy(errmsg, "LinkTable not initialized", sizeof(errmsg));
		return -2;
	}

	EmptyRecyclePool();
	header->dwRecyclePoolSize = sz;

	return 0;
}

// ����ͷ��Ԥ�����û��ռ�
int LinkTable::GetHeaderData(void *pBuff, int iSize)
{
	if(iSize<=0 || pBuff==NULL)
	{
		strncpy(errmsg, "Invalid argument", sizeof(errmsg));
		return -1;
	}
	if(header==NULL || blocks==NULL)
	{
		strncpy(errmsg, "LinkTable not initialized", sizeof(errmsg));
		return -2;
	}

    if(iSize>(int)header->dwUserDataLen)
        iSize = (int)header->dwUserDataLen;

    memcpy(pBuff, (void*)header->sUserData, iSize);
    return iSize;
}

int LinkTable::SetHeaderData(const void *pData, int iLen)
{
	if(iLen<0 || (iLen&&pData==NULL))
	{
		strncpy(errmsg, "Invalid argument", sizeof(errmsg));
		return -1;
	}
	if(header==NULL || blocks==NULL)
	{
		strncpy(errmsg, "LinkTable not initialized", sizeof(errmsg));
		return -2;
	}
    if(iLen>(int)sizeof(header->sUserData))
        iLen = (int)sizeof(header->sUserData);

    if(iLen>0)
        memcpy((void*)header->sUserData, pData, iLen);

    header->dwUserDataLen = iLen;
    return iLen;
}

//����
int LinkTable::PrintInfo(const string &prefix)
{
    long i=0;
    unsigned long total=0;

    if(header==NULL || blocks==NULL)
    {
        cout << prefix << "Linktable not initialized" << endl;
        return -1;
    }

    cout << prefix << "ddwAllBlockCount:  " << header->ddwAllBlockCount << endl;
    cout << prefix << "ddwFreeBlockCount: " << header->ddwFreeBlockCount << endl;
    cout << prefix << "ddwBlockSize:      " << header->ddwBlockSize << endl;
    cout << prefix << "ddwFirstFreePos:     " << header->ddwFirstFreePos << endl;
    cout << prefix << "ddwRecycleIndex:     " << header->ddwRecycleIndex << endl;

    for(total=0,i=0; i<header->dwRecyclePoolSize; i++)
    {
        if(header->addwRecyclePool[i])
            total ++;
    }
    cout << prefix << "dwRecycledCount:     " << total << endl;
    cout << prefix << "dwTableUsage:        " << GetUsage() << endl;

    return 0;
}

int LinkTable::ReportInfo()
{
    unsigned long i, total=0;

    if(header==NULL || blocks==NULL)
        return -1;

    Attr_API_Set(389162, header->ddwAllBlockCount);
    Attr_API_Set(389163, header->ddwFreeBlockCount);
    Attr_API_Set(389164, header->ddwBlockSize);
    Attr_API_Set(389165, header->ddwFirstFreePos);
    Attr_API_Set(389166, header->ddwRecycleIndex);
    for(total=0,i=0; i<header->dwRecyclePoolSize; i++)
    {
        if(header->addwRecyclePool[i])
            total ++;
    }
    Attr_API_Set(389167, total);
    Attr_API_Set(389168, GetUsage());

    return 0;
}



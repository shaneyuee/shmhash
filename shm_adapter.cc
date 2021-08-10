#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include "shm_adapter.h"

static const char *errnotostr(int errnum)
{
	static __thread char s[64];
	return strerror_r(errno, s, sizeof(s));
}

/*
 * Ĭ�Ϲ��캯��
 */
shm_adapter::shm_adapter()
{
	m_bIsNewShm = false;
	m_ddwShmKey = 0;
	m_ddwShmSize = 0;
	m_bIsValid = false;
	m_pShm = NULL;
	m_nShmId = 0;
}

/*
 * ���캯��
 */
shm_adapter::shm_adapter(uint64_t ddwShmKey, uint64_t ddwShmSize, bool bCreate, bool bReadOnly, bool bLock, bool bHugePage, bool bInitClear)
{
	int iRet;
	m_bIsNewShm = false;
	m_ddwShmKey = ddwShmKey;
	m_ddwShmSize = ddwShmSize;
	m_bIsValid = false;
	m_pShm = NULL;
	int nFlag = 0666;	//Ĭ�ϱ�־
	if(bCreate)
	{
		//���Ӵ�����־
		nFlag |= IPC_CREAT;
	}
	else if(bReadOnly)
	{
		//�Ǵ���, ����ֻ��, ȥ��д��־
		nFlag &= 077777444; 
	}
	if(bHugePage)
	{
		//���Ӵ�ҳ��־
		nFlag |= SHM_HUGETLB;
	}
	//��ȡShmId
	int iShmId = shmget(ddwShmKey, ddwShmSize, nFlag & (~IPC_CREAT));
	if(iShmId < 0)
	{
		//����ʧ��, �����ǷǴ�����, ����ʧ��
		if(! (nFlag & IPC_CREAT))
		{
			m_strErrMsg = "shmget without create failed - "; m_strErrMsg += errnotostr(errno);
			return;
		}
		//����ʧ��, �����ù����ڴ�
		m_bIsNewShm = true;
		iShmId = shmget(ddwShmKey, ddwShmSize , nFlag);
		if(iShmId < 0)
		{
			char err[1024];
			snprintf(err, sizeof(err), "shmget when creating shm key=0x%lX, size=%lu failed: %s", ddwShmKey, ddwShmSize, errnotostr(errno));
			m_strErrMsg = err;
			return;
		}
		//Attach
		m_pShm = shmat(iShmId, NULL, 0);
		if(m_pShm == (void *)-1)
		{
			m_strErrMsg = "shmat with create failed - "; m_strErrMsg += errnotostr(errno);
			m_pShm = NULL;
			return;
		}
		//�����ɹ����ʼ��Ϊ0
		if(bInitClear)
		{
			memset(m_pShm, 0, ddwShmSize);
		}
	}
	else 
	{
		//�����ڴ����, Attach
		m_pShm = shmat(iShmId, NULL, 0);
		if(m_pShm == (void *)-1)
		{
			m_strErrMsg = "shmat with exist shm failed - "; m_strErrMsg += errnotostr(errno);
			m_pShm = NULL;
			return;
		}
	}

	m_nShmId = iShmId;
	
	//��ס�ڴ�
	if(bLock)
	{
		iRet = mlock(m_pShm, m_ddwShmSize);
		if(iRet != 0)
		{
			m_strErrMsg = "mlock failed - "; m_strErrMsg += errnotostr(errno);
			//����ʧ��, dettach�ڴ�
			shmdt(m_pShm);
			m_pShm = NULL;
			return;
		}
	}
	m_bIsValid = true;
}

/*
 * ��������
 */
shm_adapter::~shm_adapter()
{
	if(m_pShm)
	{
		shmdt(m_pShm);
	}
}

/*
 * ˢ�¹����ڴ�
 */
bool shm_adapter::refresh()
{
	if(! m_pShm)
	{
		m_strErrMsg = "shm is not init yet";
		return false;
	}

	struct shmid_ds stShmStat;
	//�������ӹ����ڴ�
	int nShmId = shmget(m_ddwShmKey, 0, 0); 
	if(nShmId < 0)
	{
		m_strErrMsg = "shmget failed - "; m_strErrMsg += errnotostr(errno);
		return false;
	}
	//ShmIdһ��, �����ڴ�û�з����仯
	if(nShmId == m_nShmId)
	{
		return true;
	}
	//��ȡ�����ڴ��С
	int iRet = shmctl(nShmId, IPC_STAT, &stShmStat);
	if(iRet < 0)
	{
		m_strErrMsg = "shmctl failed - "; m_strErrMsg += errnotostr(errno);
		return false;
	}
	//��Сһ��, û�з����仯
	if(m_ddwShmSize == stShmStat.shm_segsz)
	{
		return true;
	}

	//����Attach�����ڴ�
	void *pShm = shmat(nShmId, NULL, 0);
	if(pShm == (void *)-1)
	{
		m_strErrMsg = "shmat failed - "; m_strErrMsg += errnotostr(errno);
		return false;
	}

	//�ͷžɵĹ����ڴ�����
	shmdt(m_pShm);

	m_pShm = pShm;
	m_nShmId = nShmId;
	m_ddwShmSize = stShmStat.shm_segsz;

	return true;
}

/*
 * �򿪹����ڴ�
 */
bool shm_adapter::open(uint64_t ddwShmKey, bool bReadOnly)
{
	if(m_pShm)
	{
		m_strErrMsg = "Shm is NOT null, you have opened one already";
		return false;
	}
	struct shmid_ds stShmStat;
	//���ӹ����ڴ�
	int nShmId = shmget(ddwShmKey, 0, 0);
	if(nShmId < 0)
	{
		m_strErrMsg = "shmget failed - "; m_strErrMsg += errnotostr(errno);
		return false;
	}
	//��ȡ�����ڴ��С
	int iRet = shmctl(nShmId, IPC_STAT, &stShmStat);
	if(iRet < 0)
	{
		m_strErrMsg = "shmctl to get size failed - "; m_strErrMsg += errnotostr(errno);
		return false;
	}
	//Attach�����ڴ�
	if(bReadOnly)
	{
		m_pShm = shmat(nShmId, NULL, SHM_RDONLY);
	}
	else
	{
		m_pShm = shmat(nShmId, NULL, 0);
	}
	if(m_pShm == (void *)-1)
	{
		m_strErrMsg = "shmat with exist shm failed - "; m_strErrMsg += errnotostr(errno);
		m_pShm = NULL;
		return false;
	}

	m_nShmId = nShmId;
	m_ddwShmSize = stShmStat.shm_segsz;
	m_ddwShmKey = ddwShmKey;
	
	m_bIsValid = true;
	return true;
}

/*
 * ���������ڴ�
 */
bool shm_adapter::create(uint64_t ddwShmKey, uint64_t ddwShmSize, bool bHugePage, bool bInitClear)
{
	if(m_pShm)
	{
		m_strErrMsg = "Shm is NOT null, you have opened one already";
		return false;
	}
	//���ô�����־
	int nFlag = 0666 | IPC_CREAT;
	if(bHugePage)
	{
		nFlag |= SHM_HUGETLB;
	}
	//�������ӹ����ڴ�
	int nShmId = shmget(ddwShmKey, 0, 0);
	if(nShmId < 0)
	{
		//����ʧ��, ���������ڴ�
		nShmId = shmget(ddwShmKey, ddwShmSize, nFlag);
		if(nShmId < 0)
		{
			char err[1024];
			snprintf(err, sizeof(err), "shmget when creating shm key=0x%lX, size=%lu failed: %s", ddwShmKey, ddwShmSize, errnotostr(errno));
			m_strErrMsg = err;
			return false;
		}
		//Attach�����ڴ�
		m_pShm = shmat(nShmId, NULL, 0);
		if(m_pShm == (void *)-1)
		{
			m_strErrMsg = "shmat failed - "; m_strErrMsg += errnotostr(errno);
			m_pShm = NULL;
			return false;
		}
		if(bInitClear)
		{
			memset(m_pShm, 0, ddwShmSize);
		}
		m_bIsNewShm = true;
	}
	else
	{
		//�����ڴ��Ѿ�����, �жϴ�С�Ƿ�һ��
		struct shmid_ds stShmStat;
		int iRet = shmctl(nShmId, IPC_STAT, &stShmStat);
		if(iRet < 0)
		{
			m_strErrMsg = "shmctl with size failed - "; m_strErrMsg += errnotostr(errno);
			return false;
		}
		//��С��һ��
		if(stShmStat.shm_segsz != ddwShmSize)
		{
			m_strErrMsg = "shm exist, but size is not the same";
			return false;
		}
		//Attach�����ڴ�
		m_pShm = shmat(nShmId, NULL, 0);
		if(m_pShm == (void *)-1)
		{
			m_strErrMsg = "shmat failed - "; m_strErrMsg += errnotostr(errno);
			m_pShm = NULL;
			return false;
		}
		m_bIsNewShm = false;
	}
	m_nShmId = nShmId;
	m_ddwShmKey = ddwShmKey;
	m_ddwShmSize = ddwShmSize;
	m_bIsValid = true;
	return true;
}

/*
 * �رչ����ڴ�
 */
bool shm_adapter::close()
{
	if(m_pShm)
	{
		//Dettach�����ڴ�
		int iRet = shmdt(m_pShm);
		if(iRet < 0)
		{
			m_strErrMsg = "shmdt shm failed - "; m_strErrMsg += errnotostr(errno);
			return false;
		}
		m_pShm = NULL;
	}
	m_bIsValid = false;
	return true;
}

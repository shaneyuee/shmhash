#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/shm.h>

//�����ڴ���
class shm_adapter
{
	public:
		/*
		 * Ĭ�Ϲ��캯��
		 * ��Ҫʹ��create/open����������򿪹����ڴ�	
		 */
		shm_adapter();

		/*
		 * ���캯��
		 * ��Ҫʹ��is_valid�����ж��Ƿ�ɹ�
		 * ע: bLock��ס�ڴ�, ֻ��root�û�����ʹ��
		 */
		shm_adapter(uint64_t ddwShmKey, uint64_t ddwShmSize, bool bCreate = true, bool bReadOnly = false, bool bLock = true, bool bHugePage = false, bool bInitClear = true);

		/*
		 * ��������
		 */
		~shm_adapter();

		/*
		 * �жϳ�ʼ���Ƿ�ɹ�
		 */
		bool is_valid() { return m_bIsValid; }

		/*
		 * ��ȡ����/���ӵĹ����ڴ�
		 */
		void *get_shm() { return m_pShm; }

		/*
		 * ˢ�¹����ڴ�
		 * ע:	���øú�����ʹ��shmdt, �������û�Ӱ������
		 *		��������ڴ治���ڳ��������ڼ䱻����ĳ������´���/�޸Ĵ�С, ����Ҫ���øú���
		 */
		bool refresh();

		/*
		 * ȡ��ʧ����Ϣ
		 */
		const std::string &get_err_msg() { return m_strErrMsg; }

		/*
		 * �򿪴��ڵĹ����ڴ�
		 */
		bool open(uint64_t ddwShmKey, bool bReadOnly = false);

		/*
		 * ֻ���򿪴��ڵĹ����ڴ�
		 */
		bool open_readonly(uint64_t ddwShmKey) { return open(ddwShmKey, true); }

		/*
		 * ���������ڴ�
		 */
		bool create(uint64_t ddwShmKey, uint64_t ddwShmSize, bool bHugePage = false, bool bInitClear = true);

		/*
		 * ʹ�ô�ҳ���������ڴ�
		 */
		bool create_hugepage(uint64_t ddwShmKey, uint64_t ddwShmSize, bool bInitClear = true) { return create(ddwShmKey, ddwShmSize, true, bInitClear); }

		/*
		 * ��ȡ�����ڴ�Ĵ�С
		 */
		uint64_t get_size() { return m_ddwShmSize; }
		
		/*
		 * ��ȡ�����ڴ��Key
		 */
		uint64_t get_key() { return m_ddwShmKey; }

		/*
		 * �ж��Ƿ�Ϊ�µĹ����ڴ�
		 */
		bool is_new() { return m_bIsNewShm; }

		/*
		 * �رչ����ڴ�
		 */
		bool close();

	private:

	public:

	private:

		bool m_bIsNewShm;		//�Ƿ����´����Ĺ����ڴ�

		uint64_t m_ddwShmKey;	//�����ڴ��Key
		uint64_t m_ddwShmSize;	//�����ڴ�Ĵ�С
		int m_nShmId;			//�����ڴ��ID

		bool m_bIsValid;		//�����ڴ��Ƿ����

		void *m_pShm;			//�����ڴ��ָ��

		std::string m_strErrMsg;	//������Ϣ
};

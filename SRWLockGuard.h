#pragma once
/*-------RAII--------
	  LockGuard
-------------------*/

class SRWLockGuard
{
public:
	SRWLockGuard(SRWLOCK* lock) : _playerLock(lock) { AcquireSRWLockExclusive(_playerLock); }
	~SRWLockGuard() { ReleaseSRWLockExclusive(_playerLock); }

	SRWLOCK* _playerLock;
};

class SRWSharedLockGuard
{
public:
	SRWSharedLockGuard(SRWLOCK* lock) : _playerLock(lock) { AcquireSRWLockShared(_playerLock); }
	~SRWSharedLockGuard() { ReleaseSRWLockShared(_playerLock); }

private:
	SRWLOCK* _playerLock;

};
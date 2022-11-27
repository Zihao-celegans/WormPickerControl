/*
	StrictLock.h
	Anthony Fouad
	Class for using mutex locks in both internal and external fashions.

	Adapted from:
	https://www.boost.org/doc/libs/1_58_0/doc/html/thread/synchronization.html#thread.synchronization.mutex_concepts

	See code and examples under "Locks as Permits"

	Note that this fashion of locking ensures that SOME <X> object is locked (e.g. SOME camera object) but it 
	doesn't ensure that a SPECIFIC <camera> object is locked. Use uniqe_lock if that level of control is needed, 
	and/or use of the owns_lock member function.
		
		For external locking, you have to create a StrictLock object. It's not absolutely necessary for the
		object to be passed to member functions, but having it as an input argument to member functions 
		helps enforce at compile time that a strict lock object of the correct type was created. note again
		that it doesn't ensure that the CORRECT object is locked, that's up to the programmer. 

	For this implementation the programmer must be careful to lock the correct object.

	Can's and Can't's:

	You CAN (only) create a StrictLock<T> starting from a valid T object.
		BankAccount myAccount("John Doe", "123-45-6789");
		StrictLock<BankAccount> myLock(myAccount); // ok

	You CAN (only) pass a StrictLock by reference to and from functions.
		// ok, Foo returns a reference to strict_lock<BankAccount>
		extern strict_lock<BankAccount>& Foo();
		// ok, Bar takes a reference to strict_lock<BankAccount>
		extern void Bar(strict_lock<BankAccount>&);

	You CANNOT copy a StrictLock or pass as a const ref - the copy constructor is private.
		extern strict_lock<BankAccount> Foo(); // compile-time error
		extern void Bar(strict_lock<BankAccount>); // compile-time error


*/
#pragma once
#ifndef STRICTLOCK_H
#define STRICTLOCK_H

// Boost includes
#include "boost\thread.hpp"
#include "boost\thread\lockable_adapter.hpp"
#include "boost\thread\lock_guard.hpp"

template <typename Lockable>
class StrictLock {
public:
	typedef Lockable lockable_type;

	explicit StrictLock(lockable_type& obj) : obj_(obj) {
		obj.lock(); // locks on construction
	}
	StrictLock() = delete;
	StrictLock(StrictLock const&) = delete;
	StrictLock& operator=(StrictLock const&) = delete;

	~StrictLock() { obj_.unlock(); } //  unlocks on destruction 

	// Manual lock and unlock
	void lock() { 
		obj_.lock(); 
	}
	void unlock(){ obj_.unlock(); }

	// Returns a reference to the locked object
	//bool owns_lock(boost::lock_guard::mutex_type(Lockable *mtx)) const // strict lockers specific function 
	//{
	//	return mtx == &obj_;
	//}
private:
	lockable_type & obj_;
};

#endif
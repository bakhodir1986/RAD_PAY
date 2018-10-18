
#if 0
#pragma once
#include <vector>
#include <boost/thread.hpp>
#include "types.h"

#define DEFAULT_SIZE 0xFFFF

template<typename el_type>
class Queue_T
{
public:
	Queue_T(void){
		count = DEFAULT_SIZE;
		buffer.resize( count );
		begin = 0;
		end = 0;
		m_size = 0;
		cross = false;
	}
    ~Queue_T(void){}

    bool put(el_type value)
	{
        boost::mutex::scoped_lock lock(mtx);
		if( !cross && end - begin >= count - 1 || cross && begin - end == 1 )
			return false;

		buffer[end] = value;
		end++;
		if( end == count ){
			end = 0;
			cross = true;
		}
		if( end == begin ) {
			return false;
		}
		m_size++;
		return true;
	}
	bool get( el_type & value )
	{
		boost::mutex::scoped_lock lock( mtx );
		if( begin == end )
			return false;

		value = buffer[begin];
		begin++;
		if( begin == count ) {
			begin = 0;
			cross = false;
		}

		m_size--;
		return true;
	}

	uint32_t size() {
		boost::mutex::scoped_lock lock( mtx );
		return m_size;
	}

	uint32_t max_size() {
		return count;
	}

	bool empty() {
		return m_size==0;
	}

private:
	uint32_t m_size;
	uint32_t count;
	uint32_t begin;
	uint32_t end;
	bool cross;
	std::vector<el_type> buffer;

	boost::mutex mtx;
};
#endif 


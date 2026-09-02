#pragma once

#include <cstdint>
#include <scl/basis/errhandle.hpp>

namespace scl {

typedef struct poolStatus {
	uint32_t mAlive;
	uint16_t mID,mIDNext;
	constexpr bool alive() const { return mAlive; }
	constexpr std::size_t id() const { return mID; }
} poolStatus;

template <typename T> class pool {
	public:
		T* mObjects;
		poolStatus* mStatus;
		bool mAutoEnable;
		size_t mAliveNow;
		size_t mAliveMax;
		size_t mIdxLast;

		// construct/destructor -------------------------@/
		pool() {
			mObjects = nullptr;
			mStatus = nullptr;
			mAutoEnable = false;
			mIdxLast = 0;
			mAliveNow = 0;
			mAliveMax = 0;
		}
		pool(size_t max) {
			mObjects = nullptr;
			mStatus = nullptr;
			mAutoEnable = true;
			mIdxLast = 0;
			mAliveNow = 0;
			mAliveMax = max;

			mObjects = static_cast<T*>(std::malloc(sizeof(T) * mAliveMax));
			mStatus = new poolStatus[max];

			setup();
		}
		pool(T* objects, poolStatus* status, size_t max) {
			mObjects = nullptr;
			mStatus = nullptr;
			mAutoEnable = false;
			mIdxLast = 0;
			mAliveNow = 0;
			mAliveMax = max;

			mObjects = objects;
			mStatus = status;

			setup();
		}

		~pool() {
			if(mObjects) {
				std::free(mObjects);
				mObjects = nullptr;
			}
			if(mStatus) {
				delete[] mStatus;
				mStatus = nullptr;
			}

			mIdxLast = 0;
			mAliveNow = 0;
			mAliveMax = 0;
		}

	private:
		auto setup() -> void {
			SCL_ASSERT_MSG(mAliveMax>0,"pool %p: max alive must be > 0",this);
			SCL_ASSERT_MSG(mAliveNow==0,"pool %p: num. alive must be 0",this);

			if(mAutoEnable) {
				SCL_ASSERT_MSG(mObjects,"pool %p: objects must exist",this);
				SCL_ASSERT_MSG(mStatus,"pool %p: status must exist",this);
			}
			
			for(size_t i=0; i<mAliveMax; i++) {
				auto &status = mStatus[i];
				status.mAlive = false;
				status.mID = i & 0xFFFF;
				status.mIDNext = (i+1) & 0xFFFF;
			}
			mStatus[mAliveMax-1].mID = 0xFFFF;
		}
};
	
} // namespace scl

#pragma once
#include "lightspeed/base/streams/fileio_ifc.h"
#include "lightspeed/utils/FilePath.h"
#include "lightspeed/base/containers/optional.h"
#include "lightspeed/base/containers/constStr.h"
#include <algorithm>
#include "../libs/lightspeed/src/lightspeed/base/streams/fileiobuff.h"

namespace SourceIndex {

	using namespace LightSpeed;

	///Creates binary map from file
	/** Object expects, that map is sorted by Order
	 *
	 * Key and Value must be binary or POD type */

	template<typename Key, typename Value, typename Order>
	class BinMap: public ConstStringT<std::pair<Key,Value> >
	{
	public:
		typedef std::pair<Key, Value> KeyValue;
		typedef ConstStringT<KeyValue> Super;

		BinMap() {}
		explicit BinMap(Order order) :order(order) {}

		void open(FilePath fname);
		void close();
		FilePath getName() const;

		class Iterator : public IteratorBase<KeyValue, Iterator> {
		public:
			Iterator() {}
			Iterator(ConstStringT<KeyValue> section, const Key &keySearchEnd, const Order &order)
				:section(section), keySearchEnd(keySearchEnd), index(0), order(&order) {}
			const KeyValue &getNext() {
				const KeyValue &ret = peek();
				index++;				
				return ret;
			}
			const KeyValue &peek() const {
				return section[index];
			}
			bool hasItems() const {
				return index < section.length() && !(*order)(keySearchEnd, section[index].first);
			}
		protected:
			ConstStringT<KeyValue> section;
			Key keySearchEnd;
			natural index;
			const Order *order;
		};

		Iterator find(const Key &key) const;
		Iterator findRange(const Key &lower, const Key &upper) const;

		///Merges two maps
		/**
		  Merges map a with map b and result stores into current map. Current map must be initialized either by open or load or init
		  @param a map a
		  @param b map b
		*/
		void merge(const BinMap &a, const BinMap &b);

		///Merges two maps with respect ordering of values. It also removes duplicate key-value pairs
		/**
			Merges map a with map b and result stores into current map. Current map must be initialized either by open or load or init
			@param a map a
			@param b map b
			@param vorder value ordering operator
			*/
		template<typename ValueOrder>
		void merge(const BinMap &a, const BinMap &b, const ValueOrder &vorder);

		///initialized map
		/** loads data to map file. It can also order data if requested */
		void load(FilePath fname, const Super &data, bool alreadyOrdered = false);
		void init(FilePath fname);
		void loadToMemory(const Super &data, bool alreadyOrdered = false);

		

	protected:
		
		FilePath fname;
		PMappedFile mfile;
		Optional<IMappedFile::MappedRegion> mregion;		
		Order order;

		class SortOrder {
		public:
			SortOrder(const Order &x) :x(x) {}
			bool operator()(const KeyValue &a, const KeyValue &b) const {
				return x(a.first, b.first);
			}

			const Order &x;
		};

	};

	template<typename Key, typename Value, typename Order>
	void SourceIndex::BinMap<Key, Value, Order>::loadToMemory(const Super &data, bool alreadyOrdered /*= false*/)
	{		

		class SimuMappedFile : public IMappedFile {
		public:

			SimuMappedFile(natural size) :file(0, size) {}

			virtual MappedRegion map(IFileIOServices::FileOpenMode mode, bool copyOnWrite) override {
				return MappedRegion(this, file.data(), file.length(), 0);
			}

			virtual MappedRegion map(IRndFileHandle::FileOffset offset, natural size, IFileIOServices::FileOpenMode mode, bool copyOnWrite) override {
				return MappedRegion(this, file.data() + offset, size, offset);
			}

			virtual void unmap(MappedRegion &reg) override	{}
			virtual void sync(MappedRegion &reg) override {}
			virtual void lock(MappedRegion &reg) override {}
			virtual void unlock(MappedRegion &reg) override {}

			AutoArray<byte> file;

		};
		natural size = data.length()*sizeof(KeyValue);
		SimuMappedFile *m = new SimuMappedFile(size);
		this->mfile = m;

		if (alreadyOrdered)  {
			memcpy(m->file.data(), data.data(), size);
		}
		else  {
			AutoArray<KeyValue> buffer(data);
			std::sort(buffer.data(), buffer.data() + buffer.length(), SortOrder(order));
			memcpy(m->file.data(), buffer.data(), size);
		}

	}

	template<typename Key, typename Value, typename Order>
	void SourceIndex::BinMap<Key, Value, Order>::init(FilePath fname)
	{
		Super::operator=(Super());
		this->fname = fname;
	}

	template<typename Key, typename Value, typename Order>
	void SourceIndex::BinMap<Key, Value, Order>::load(FilePath fname, const Super &data, bool alreadyOrdered /*= false*/)
	{
		if (alreadyOrdered)  {			
			SeqFileOutput f(fname, OpenFlags::create | OpenFlags::truncate | OpenFlags::commitOnClose | OpenFlags::accessSeq);
			f.blockWrite(data.data(), data.length(), true);
			close();
		}
		else  {
			AutoArray<KeyValue> buffer(data);
			std::sort(buffer.data(), buffer.data() + buffer.length(), SortOrder(order));
			return load(fname, buffer, true);
		}
		open(fname);
	}

	template<typename Key, typename Value, typename Order>
	void SourceIndex::BinMap<Key, Value, Order>::merge(const BinMap &a, const BinMap &b)
	{
		SeqFileOutBuff<> outFile(fname, OpenFlags::create | OpenFlags::truncate | OpenFlags::commitOnClose | OpenFlags::accessSeq);
		Super::Iterator ia = a.getFwIter();
		Super::Iterator ib = b.getFwIter();
		while (ia.hasItems() && ib.hasItems()) {
			const KeyValue &ap = ia.peek();
			const KeyValue &bp = ib.peek();
			if (order(bp.first, ap.first)) {
				outFile.blockWrite(&ib.getNext(), sizeof(KeyValue), true);				
			}			
			else {
				outFile.blockWrite(&ia.getNext(), sizeof(KeyValue), true);
			}
		}
		while (ia.hasItems()) {
			outFile.blockWrite(&ia.getNext(), sizeof(KeyValue), true);
		}
		while (ib.hasItems()) {
			outFile.blockWrite(&ib.getNext(), sizeof(KeyValue), true);
		}
		close();
		outFile.closeOutput();
		open(fname);
	}


	template<typename Key, typename Value, typename Order>
	template<typename ValueOrder>
	void SourceIndex::BinMap<Key, Value, Order>::merge(const BinMap &a, const BinMap &b, const ValueOrder &vorder)
	{
		SeqFileOutBuff<> outFile(fname, OpenFlags::create | OpenFlags::truncate | OpenFlags::commitOnClose | OpenFlags::accessSeq);
		Super::Iterator ia = a.getFwIter();
		Super::Iterator ib = b.getFwIter();
		while (ia.hasItems() && ib.hasItems()) {
			const KeyValue &ap = ia.peek();
			const KeyValue &bp = ib.peek();
			if (order(bp.first, ap.first)) {
				outFile.blockWrite(&ib.getNext(), sizeof(KeyValue), true);
			}
			else if (order(ap.first, bp.first)) {			
				outFile.blockWrite(&ia.getNext(), sizeof(KeyValue), true);
			}
			else if (vorder(bp.second,ap.second)) {
				outFile.blockWrite(&ib.getNext(), sizeof(KeyValue), true);				
			}
			else if (vorder(ap.second, bp.second)) {			
				outFile.blockWrite(&ia.getNext(), sizeof(KeyValue), true);
			}
			else {
				outFile.blockWrite(&ia.getNext(), sizeof(KeyValue), true);
			}
		}
		while (ia.hasItems()) {
			outFile.blockWrite(&ia.getNext(), sizeof(KeyValue), true);
		}
		while (ib.hasItems()) {
			outFile.blockWrite(&ib.getNext(), sizeof(KeyValue), true);
		}
		close();
		outFile.closeOutput();
		open(fname);

	}


	template<typename Key, typename Value, typename Order>
	typename SourceIndex::BinMap<Key, Value, Order>::Iterator SourceIndex::BinMap<Key, Value, Order>::find(const Key &key) const
	{
		return findRange(key, key);
	}
	template<typename Key, typename Value, typename Order>
	typename SourceIndex::BinMap<Key, Value, Order>::Iterator SourceIndex::BinMap<Key, Value, Order>::findRange(const Key &lower, const Key &upper) const
	{

		if (mregion == nil) return Iterator(ConstStringT<KeyValue>(),lower,order);
		natural start = 0;
		natural end = (mregion->size) / sizeof(KeyValue);
		natural arrend = this->length();
		const KeyValue *arr = this->data();


		while (start < end) {
			natural middle = (start + end) / 2;
			const KeyValue & item = arr[middle];
			if (order(item.first, lower)) {
				end = middle;
			}
			else {
				start = middle + 1;
			}
		}
		natural found = end;
		while (found < arrend && order(arr[found].first, lower)) {
			found++;
		}
		return Iterator(ConstStringT<KeyValue>(arr + found, arrend - found), upper,order);

	}

	template<typename Key, typename Value, typename Order>
	void BinMap<Key,Value,Order>::open(FilePath fname)
	{
		this->fname = fname;
		mfile = IFileIOServices::getIOServices().mapFile(fname, IFileIOHandler::fileOpenRead);
		mregion = mfile->map(IFileIOHandler::fileOpenRead, false);
		Super::operator=(Super(reinterpret_cast<const KeyValue *>(mregion->address), mregion->size / sizeof(KeyValue)));
		
	}

	template<typename Key, typename Value, typename Order>
	void BinMap<Key, Value, Order>::close()
	{
		mregion = nil;
		mfile = nil;
	}

	template<typename Key, typename Value, typename Order>
	FilePath BinMap<Key, Value, Order>::getName() const
	{
		return fname;
	}



}
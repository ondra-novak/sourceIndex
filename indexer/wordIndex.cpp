#include <cwctype>
#include "wordIndex.h"
#include "wordid.h"

#include <lightspeed/base/streams/utf.h>
#include "lightspeed/base/containers/map.h"
#include "lightspeed/base/containers/autoArray.tcc"
#include "lightspeed/base/text/textstream.tcc"
#include "lightspeed/base/streams/fileiobuff.tcc"
#include "lightspeed/base/debug/dbglog.h"
#include "lightspeed/base/containers/convertString.h"
#include "lightspeed/utils/base64.tcc"
#include "lightspeed/base/exceptions/fileExceptions.h"
#include "lightspeed/utils/base64.h"

namespace SourceIndex {



	WordIndex::WordIndex(const FilePath &path, const FilePath &wordIndexMap) 
		:path(path)
		, wordIndexFile(wordIndexMap)
		, svc(IFileIOServices::getIOServices())
	{

	}

	void WordIndex::checkPath()
	{
		if (!svc.canOpenFile(path.getPath(),IFileIOHandler::fileAccessible))
			svc.createFolder(path.getPath(), true);
	}


	void WordIndex::init()
	{
		checkPath();
		if (svc.canOpenFile(wordIndexFile, IFileIOHandler::fileAccessible))
			wordIndex.open(wordIndexFile);
		else
			wordIndex.init(wordIndexFile);

	}

	void WordIndex::commit()
	{
		WordIndexMap localMap;
		localMap.loadToMemory(newWordIndex);
		wordIndex.merge(wordIndex, localMap, std::less<WordID>());
	}

	void WordIndex::addToIndex(const TokenizedDocument &doc)
	{
		typedef MultiMap<WordID, natural> MatchMap;
		MatchMap matchMap;
		for (natural i = 0; i < doc.docIncrement.length(); i++) {
			matchMap.insert(doc.docIncrement[i].wordID, i);
		}
		for (MatchMap::KeyIter kiter = matchMap.getKeyFwIter(); kiter.hasItems();) {
			const MatchMap::KeyEntity &ke = kiter.getNext();
			WordID wid = ke.key;
			const MatchMap::ValueList  &vl = ke.value;
			SeqFileOutput wordIndex = openWordIndexForWrite(wid);
			SeqFileOutBuff<> wordIndexBuff(wordIndex);
			if (vl.size() > 65535) {
				writeDocHeader(wordIndexBuff, doc.docId, 0);
			} else {
				writeDocHeader(wordIndexBuff, doc.docId, vl.size());
				for (MatchMap::ValueList::Iterator viter = vl.getFwIter(); viter.hasItems();) {
					natural ofs = viter.getNext();
					writeMatchHit(wordIndexBuff, doc.docIncrement[ofs].match);
				}
			}
		}
		for (Set<StringPoolA::Str>::Iterator iter = doc.wordList.getFwIter(); iter.hasItems();) {
			ConstStrA word = iter.getNext();
			WordID wordId = getWordId(word);
			bool exists = false;
			newWordConstrain.insert(wordId, &exists);
			if (!exists) {
				indexNewWord(word, wordId);
			}
		}

	}

	void WordIndex::writeDocHeader(SeqFileOutput wordIndex, DocID docId, Bin::natural16 count)
	{
		wordIndex.blockWrite(docId, true);
		wordIndex.blockWrite(&count, 2, true);

		
	}


	LightSpeed::SeqFileOutput WordIndex::openWordIndexForWrite(WordID wid)
	{
		FilePath p = getWordIndexPathname(wid);
		try {
			SeqFileOutput out(p, OpenFlags::append);
			return out;
		}
		catch (FileOpenError &) {
			SeqFileOutput out(p, OpenFlags::create | OpenFlags::newFile|OpenFlags::createFolder);
			return out;
		}
	}



	SourceIndex::OpenedWordIndexFile WordIndex::openWordIndexForRead(WordID wid) const
	{
		FilePath path = getWordIndexPathname(wid);
		return OpenedWordIndexFile(path, svc);
	}

	FilePath WordIndex::getWordIndexPathname(WordID wid) const
	{
		StringA name = convertString(Base64Encoder(true, true), ConstBin(reinterpret_cast<const byte *>(&wid), sizeof(wid)));
		StringA subdir = convertString(HexEncoder<>(), ConstBin(reinterpret_cast<const byte *>(&wid), 1));
		String fname = name + ConstStrA(".idx");
		FilePath pathname = path / subdir / fname;		
		return pathname;

	}

	void WordIndex::writeMatchHit(SeqFileOutput &wordIndex, const WordMatch &wm)
	{
		wordIndex.blockWrite(&wm, sizeof(wm), true);
	}


	void WordIndex::findWords(WordID word, Flags flags, IFindWordCB *cb)
	{
		
		if (flags == flgLowerCase) {
			WordIndexMap::Iterator iter = wordIndex.findRange(WordIndexKey(word, 0), WordIndexKey(word, 0xFF));
			while (iter.hasItems()) {
				const KeyValue &kv = iter.getNext();
				cb->foundWord(kv.second, kv.first.flags);
			}
		}
		else if (flags == 0) {
			WordIndexMap::Iterator iter = wordIndex.findRange(WordIndexKey(word, 0), WordIndexKey(word, flgWordBegin|flgWordEnd));
			while (iter.hasItems()) {
				const KeyValue &kv = iter.getNext();
				cb->foundWord(kv.second, kv.first.flags);
			}
		}
		else {
			if (flags & flgLowerCase) {
				WordIndexMap::Iterator iter = wordIndex.find(WordIndexKey(word, flags));
				while (iter.hasItems()) {
					const KeyValue &kv = iter.getNext();
					cb->foundWord(kv.second, kv.first.flags);
				}
			}
			WordIndexMap::Iterator iter = wordIndex.find(WordIndexKey(word, flags & ~flgLowerCase));
			while (iter.hasItems()) {
				const KeyValue &kv = iter.getNext();
				cb->foundWord(kv.second, kv.first.flags);
			}
		}
	}

	void OpenedWordIndexFile::copyDocRecord(SeqFileOutput &wordIndex, DocID docId, Bin::natural16 count, const WordMatch *matches)
	{
		WordIndex::writeDocHeader(wordIndex, docId, count);
		for (Bin::natural16 i = 0; i < count; i++) WordIndex::writeMatchHit(wordIndex, matches[i]);
	}


	template<typename PFI>
	bool WordIndex::enumIndexVtSt(PFI iter, const IEnumIndexCb &cb) {
		while (iter->getNext()) {
			if (iter->isDot()) continue;
			else if (iter->isDirectory()) {
				if (!enumIndexVtSt(iter->openFolder(), cb)) return false;
			}
			else if (iter->entryName.tail(4) == ConstStrW(L".idx")) {
				OpenedWordIndexFile owif(FilePath(iter->getFullPath()), svc);
				if (!cb(owif)) return false;				
			}
		}
		return true;
	}

	void WordIndex::indexNewWord(ConstStrA word, WordID wordId)
	{
		const natural minSubstr = 2;
		natural len = word.length();
		if (len <= minSubstr) {
			WordID lw = getLocaseWordId(word);
			if (lw != wordId) {
				newWordIndex.add(KeyValue(WordIndexKey(lw, flgLowerCase | flgWordEnd | flgWordBegin), wordId));
			}			
		}
		else {
			natural rlen = len - minSubstr;
			for (natural i = 0; i < rlen; i++) {
				for (natural j = 1; j <= rlen - i; j++) {
					ConstStrA substr = word.mid(i, j + minSubstr);
					Flags flg;
					if (i == 0) flg |= flgWordBegin;
					if (i + j == rlen) flg |= flgWordEnd;
					WordID wid1 = getWordId(substr);
					if (wid1 != wordId) {
						newWordIndex.add(KeyValue(WordIndexKey(wid1, flg), wordId));
					}
					WordID wid2 = getLocaseWordId(substr);
					if (wid2 != wordId && wid2 != wid1) {
						newWordIndex.add(KeyValue(WordIndexKey(wid2, flg | flgLowerCase), wordId));
					}
				}
			}
		}

	}

	bool WordIndex::enumIndexVt(const IEnumIndexCb &cb)
	{
		PFolderIterator iter = svc.openFolder(path);
		return enumIndexVtSt(iter, cb);
	}

	void OpenedWordIndexFile::deleteWord()
	{
		mappedFile = NULL;
		svc.remove(wordIdxFile);
	}

	void OpenedWordIndexFile::close()
	{
		mappedFile = NULL;
	}

	OpenedWordIndexFile::OpenedWordIndexFile(const FilePath &wordIdxFile, IFileIOServices &svc)
		:wordIdxFile(wordIdxFile), svc(svc)
	{
		PFolderIterator iter = svc.getFileInfo(wordIdxFile);
		fsize = (natural)iter->getSize();
		mappedFile = iter->mapFile(IFileIOServices::fileOpenRead);
	}

	LightSpeed::SeqFileOutput OpenedWordIndexFile::openWordIndexForCleanup()
	{
		return SeqFileOutput(wordIdxFile, OpenFlags::create | OpenFlags::truncate | OpenFlags::createFolder | OpenFlags::commitOnClose);

	}

	bool OpenedWordIndexFile::enumDocumentsVt(const IEnumDocsCallback &cb) const
	{
		IMappedFile::MappedRegion rgn = mappedFile->map(IFileIOServices::fileOpenRead, false);
		ConstBin section(reinterpret_cast<const byte *>(rgn.address), fsize);

		const byte *ptr = section.data();
		const byte *endptr = ptr + section.length();
		while (ptr + sizeof(DocID) + 2 <= endptr) {
			const DocID *docId = reinterpret_cast<const DocID *>(ptr);
			ptr += 16;
			const Bin::natural16 *count = reinterpret_cast<const Bin::natural16 *>(ptr);
			ptr += 2;
			const WordMatch *wmatch = reinterpret_cast<const WordMatch *>(ptr);
			ptr += sizeof(WordMatch) * count[0];
			if (!cb(*docId, *count, wmatch)) return false;
		}
		return true;
	}

	
}


#include "lightspeed/base/containers/autoArray.tcc"
#include "lightspeed/utils/base64.tcc"
#include "lightspeed/base/containers/convertString.h"
#include "lightspeed/base/streams/fileiobuff.tcc"
#include "lightspeed/base/exceptions/fileExceptions.h"

#include "docIndex.h"

namespace SourceIndex {

	DocIndex::DocIndex(const FilePath &path)
		:path(path), svc(IFileIOServices::getIOServices())
	{
	}


	DocIndex::~DocIndex()
	{
	}

	void DocIndex::checkPath()
	{
		if (!svc.canOpenFile(path.getPath(), IFileIOHandler::fileAccessible))
			svc.createFolder(path.getPath(), true);
	}

	void DocIndex::init()
	{
		checkPath();
	}


	bool DocIndex::isInIndex(const DocID &docId)
	{
		FilePath p = getDocIndexPathname(docId);
		return svc.canOpenFile(p, IFileIOServices::fileAccessible);
	}

	void DocIndex::openDocument(const DocID &docId, DocumentMeta &dmeta) const
	{
		dmeta.docId = docId;
		dmeta.path = getDocIndexPathname(docId);
		try {
			SeqFileInBuff<> f(dmeta.path, OpenFlags::shareRead);
			dmeta.readDocument(f);
		}
		catch (FileOpenError &) {

		}
		
	}

	void DocIndex::openDocument(const FilePath &path, DocumentMeta &dmeta)
	{
		dmeta.path = path;
		dmeta.docId = getDocIDFromName(path.getTitle());
		SeqFileInBuff<> f(dmeta.path, OpenFlags::shareRead);
		dmeta.readDocument(f);

	}

	bool DocIndex::updateDocument(const DocumentMeta &dmeta)
	{
		if (dmeta.files.empty()) {
			svc.remove(dmeta.path);
			return false;
		}
		SeqFileOutput f(dmeta.path, OpenFlags::commitOnClose | OpenFlags::create | OpenFlags::createFolder | OpenFlags::truncate);
		SeqFileOutBuff<> bf(f);
		dmeta.writeDocument(bf);
		return true;
	}

	void DocIndex::initDocument(const DocumentMeta &origDoc, DocumentMeta &dmeta)
	{
		dmeta.path = origDoc.path;
		dmeta.docId = origDoc.docId;
	}

	void DocIndex::addFile(DocumentMeta &dmeta, const FileInfo &finfo)
	{
		dmeta.files.replace(finfo.fullPath, finfo.modified);
	}

	void DocIndex::DocumentRef::readDocument(SeqFileInput docFile)
	{
		AutoArray<wchar_t, SmallAlloc<1024> > buffer;
		while (docFile.hasItems()) {
			ItemHeader header;
			docFile.blockRead(&header, sizeof(header), true);
			buffer.resize(header.fnameLen);
			docFile.blockRead(buffer.data(), buffer.length()*sizeof(buffer[0]), true);			
			files.insert(String(buffer), TimeStamp(header.day, header.time));
		}
	}

	void DocIndex::DocumentRef::writeDocument(SeqFileOutput docFile) const
	{
		for (FileMap::Iterator iter = files.getFwIter(); iter.hasItems();) {
			const FileMap::Entity &e = iter.getNext();
			ItemHeader hdr;
			hdr.day = (Bin::integer32) e.value.getDay();
			hdr.time = (Bin::natural32) e.value.getTime();
			hdr.fnameLen = (Bin::natural32) e.key.length();
			docFile.blockWrite(&hdr, sizeof(hdr), true);
			docFile.blockWrite(e.key.data(), e.key.length()*sizeof(e.key[0]), true);
		}
	}

	FilePath DocIndex::getDocIndexPathname(DocID did) const
	{
		StringA name = convertString(Base64Encoder(true, true), ConstBin(did));
		StringA subdir = convertString(HexEncoder<>(), ConstBin(did[0]));
		String fname = name + ConstStrA(".idx");
		FilePath pathname = path / subdir / fname;
		return pathname;

	}

	SourceIndex::DocID DocIndex::getDocIDFromName(ConstStrW name)
	{
		DocID docId;
		Filter<Base64DecoderT<wchar_t, byte> >::Read<ConstStrW::Iterator> rd(name.getFwIter());
		for (natural i = 0; i < docId.length(); i++) docId(i) = rd.getNext();
		return docId;
	}

}

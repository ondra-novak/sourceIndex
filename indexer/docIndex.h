#pragma once
#include "lightspeed/utils/FilePath.h"
#include "lightspeed/base/containers/string.h"
#include "lightspeed/base/timestamp.h"
#include "docid.h"
#include "lightspeed/base/containers/stringpool.h"
#include "lightspeed/base/containers/map.h"
#include "fileInfo.h"

namespace SourceIndex {



	class DocIndex
	{
	public:
		DocIndex(const FilePath &path);
		~DocIndex();

		FilePath getDocIndexPathname(DocID did) const;
		static DocID getDocIDFromName(ConstStrW name);

		typedef Map<String, TimeStamp> FileMap;

		struct DocumentRef{
			FileMap files;

			void readDocument(SeqFileInput docFile);
			void writeDocument(SeqFileOutput docFile) const;
			template<typename Fn>
			bool enumFiles(Fn fn) const {
				for (FileMap::Iterator iter = files.getFwIter(); iter.hasItems();) {
					const FileMap::Entity &e = iter.getNext();
					if (!fn(FileInfo(FilePath(e.key), e.value))) return false;
				}
				return true;
			}
		};


		struct DocumentMeta : DocumentRef {
			DocID docId;
			FilePath path;
		};



		void init();
		bool isInIndex(const DocID &docId);

		void openDocument(const DocID &docId, DocumentMeta &dmeta) const;
		static void openDocument(const FilePath &path, DocumentMeta &dmeta);
		bool updateDocument(const DocumentMeta &dmeta);
		static void initDocument(const DocumentMeta &origDoc, DocumentMeta &dmeta);
		static void addFile(DocumentMeta &dmeta, const FileInfo &finfo);


		template<typename Fn>
		bool enumDocuments(Fn fn) const {
			PFolderIterator iter = svc.openFolder(path);
			return enumDocuments(fn, iter);
	
		}

	protected:
		FilePath path;
		IFileIOServices &svc;


		void checkPath();



		struct ItemHeader{
			Bin::integer32 day; 
			Bin::natural32 time;
			Bin::natural32 fnameLen;
		};

		template<typename Fn>
		bool enumDocuments(Fn fn, PFolderIterator iter) const {
			while (iter->getNext()) {
				if (iter->isDot()) continue;
				if (iter->isDirectory()) {
					if (!enumDocuments(fn, iter->openFolder())) return false;					
				}
				else {
					if (iter->entryName.tail(4) == ConstStrW(L".idx")) {
						DocumentMeta dmeta;
						openDocument(FilePath(iter->getFullPath()), dmeta);
						if (!fn(dmeta)) return false;
					}
				}
			}
			return true;
		}



	};


}

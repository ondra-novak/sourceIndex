#pragma once

#include <lightspeed\base\framework\app.h>

namespace SourceIndex {

	using namespace LightSpeed;

	class IndexerMain : public App
	{
	public:
		IndexerMain();
		~IndexerMain();

		virtual integer start(const Args &args) override;

	};



}
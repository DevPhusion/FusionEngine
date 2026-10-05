#pragma once
#include "../EditorWindow.h"            
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

class DocsWindow : public EditorWindow {
public:
	static constexpr const char* kDocsUrl = "https://devphusion.github.io/FusionEngineDocumentation/";

	DocsWindow(std::string name);
	~DocsWindow();

	void ProcessWindow() override;

	static void Open();

	static void OpenUrl(const std::string& url);

private:
	struct Run {
		std::string text;
		std::string url;          
		bool bold = false;
		bool code = false;
	};
	using Runs = std::vector<Run>;

	struct TableRow {
		bool header = false;
		std::vector<Runs> cells;
	};

	struct Block {
		enum class Type { Paragraph, Heading, Code, Table, Rule };
		Type type = Type::Paragraph;
		int level = 0;            
		int indent = 0;           
		int bullet = 0;           
		int admonition = -1;      
		Runs runs;                
		std::string code;         
		std::vector<TableRow> rows;
	};

	struct Page {
		std::string url, title, text, lower;   
		std::vector<Block> blocks;            
	};

	struct Result {
		std::shared_ptr<Page> page;
		std::string snippet;
		int score = 0;
	};

	static DocsWindow* instance;

	
	bool open = false;
	bool focusNext = false;
	char query[128] = "";
	std::string lastQuery;
	int lastPageCount = -1;
	std::vector<Result> results;

	std::shared_ptr<Page> selectedPage;
	std::vector<std::string> history;
	bool autoSelected = false;
	bool scrollToTop = false;

	std::vector<float> heights;
	float heightsWidth = 0.0f;

	std::mutex mtx;
	std::vector<std::shared_ptr<Page>> pages;
	std::string status = "Not loaded";
	std::thread worker;
	std::atomic<bool> cancel{ false };
	bool crawlStarted = false;

	void StartCrawl();
	void ReloadCrawl();
	void StopCrawl();
	void Crawl(std::string baseUrl);

	int PageCount();
	std::string Status();
	std::shared_ptr<Page> FindPage(const std::string& url);
	std::vector<Result> Search(const std::string& query, int maxResults = 500);

	static std::vector<Block> ParseBlocks(const std::string& html, const std::string& pageUrl);

	void DrawRuns(const Runs& runs, float wrap, bool forceBold, std::string& clicked);
	void DrawBlock(const Block& b, int index, std::string& clicked);
	void DrawPage(const Page& page, std::string& clicked);

	void SelectPage(const std::shared_ptr<Page>& page, bool pushHistory);
	void OpenLink(const std::string& url);
	void ResetView();
};
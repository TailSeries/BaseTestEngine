#include <Windows.h>
#include <iostream>
#include <map>
#include <timeapi.h>
#include <vector>
#include "Base4/Sort.h"
#include "LeetCode/LeetCode.h"

template<typename OutT, typename InT>
FORCEINLINE OutT value_as(InT InValue)
{
	if constexpr (std::is_same_v<OutT, InT>)
	{
		return InValue;
	}
	else if constexpr (sizeof(InT) < sizeof(OutT))
	{
		OutT ExtendedValue = {};
		memcpy(&ExtendedValue, &InValue, sizeof(InT));
		return ExtendedValue;
	}
	else
	{
		OutT OutValue;
		memcpy(&OutValue, &InValue, sizeof(OutT));
		return OutValue;
	}
}


int TestFunc002(const void* Data, const int& TypeId)
{
	return TypeId + 2;
};


void func_by_value(int typeId)
{
	int a = typeId * 2;
	// 编译器生成: mov eax, edx   (直接用寄存器值)
}

void func_by_ref(const int& typeId)
{
	int a = typeId * 2;
	// 编译器生成: mov eax, [rdx] (把 rdx 当指针解引用)
}
#include "Foo.hpp"
#include "Global.hpp"
#include <unordered_map>
extern Foo GFoo2;

double GetDouble()
{
	int a = 1;
	int b = 3;
	std::vector<int> temp{ b };
	if (0)
	{
		return b / 2.0; // 1.5
	}
	else
	{
		return  temp[0] / 2.0; //1.0
	}
};

template<typename T>
struct TAutoDereference
{
	T* Ptr;

	TAutoDereference(T* InPtr)
		: Ptr(InPtr)
	{}

	TAutoDereference(void* InPtr)
		: Ptr((T*)InPtr)
	{}


	//[juhuangjie] 对应const * 情况
	TAutoDereference(const void* InPtr)
		: Ptr((T*)InPtr)
	{}


	TAutoDereference(const T& InPtr)
		: Ptr((T*)&InPtr)
	{}

	TAutoDereference(T& InPtr)
		: Ptr(&InPtr)
	{}

	FORCEINLINE operator T& ()
	{
		return *Ptr;
	}

	FORCEINLINE operator T* ()
	{
		return Ptr;
	}

	FORCEINLINE operator void* ()
	{
		return Ptr;
	}
};

class Test002569
{
public:
	int a = 10;
};

template<typename T>
void OpAssignValue_Template(void* ValuePtr)
{
	auto it = *(T*)ValuePtr;
}



template<typename T>
struct TestTemplate001 {};
template<typename T1>
struct TestTemplate001<std::vector<T1>>
{

};

struct my_struct
{
	my_struct(bool value)
	{
		std::printf("sjdoifauaosidhf sadf\n");
	}
};

struct FEventReply
{


public:

	FEventReply(bool IsHandled = false)
		: NativeReply(IsHandled ? -1 : 1)
	{
		std::printf("FEventReply(bool IsHandled = false)");
	}
	int NativeReply = 120;
};


void Testshfiod(bool a = false)
{
	std::printf("void Test564(bool a = false)");
}

void Testshfiod(const FEventReply& av)
{
	std::printf("void Test564(FEventReply* a = nullptr)");
}

int main()
{
	Solution s;
	{


		TestTemplate001<int> vsdiou;
		int value = GFoo.a;
		SetConsoleOutputCP(CP_UTF8);

		std::vector<int> arr{ 0, 2, 1, 0, 3, 5, 6, 7, 7, 6, 8, 5 ,6 };

		BSTNode* root = PutBykey(nullptr, 0, 0);
		for (auto& itoa : arr)
		{
			BSTNode* Leaf = PutBykey(root, itoa, itoa);
		}

		std::vector<std::vector<int>> adj
		{
			{2, 1, 5},
			{0, 2},
			{0 ,1, 3, 4},
			{5,4,2},
			{3,2},
			{3, 0}
		};
		Graph G(6);
		for (int i = 0; i < adj.size(); i++)
		{
			for (int& j : adj[i])
				G.AddEdge(i, j);
		}
		G.adj = adj;
		//DFSTree dfsTree(G, 1);
		//bool result =  dfsTree.HasPathTo(0);
		//result = dfsTree.HasPathTo(4);
		//auto resultpath = dfsTree.GetPath(5);

		BFSTree bfsTree(G, 1);
		bool result = bfsTree.HasPathTo(0);
		result = bfsTree.HasPathTo(4);
		auto resultpath = bfsTree.GetPath(5);


		std::vector<int> nums = { 1000000000,1000000000,1000000000,1000000000 };
		s.fourSum(nums, 0);

		int listNum = 4;
		std::vector<Solution::ListNode> listnodes(listNum);
		std::vector<Solution::ListNode*> listnodePtrs(listNum);
		for (int i = 0; i < listNum; i++)
		{
			listnodes[i].val = i + 1;
			listnodePtrs[i] = &listnodes[i];
		}
		for (int i = 0; i < listNum - 1; i++)
		{
			listnodePtrs[i]->next = listnodePtrs[i + 1];
		}

		//s.removeNthFromEnd(listnodePtrs[0], 2);


		//s.generateParenthesis(3);
		s.strStr("mississippi", "issip");

		std::string testiosayd = "barfoothefoobarman";
		std::vector<std::string> testuiwords = { "foo", "bar" };
		s.findSubstring(testiosayd, testuiwords);

		int wordLen = 4;
		int resudsf = 0;
		for (int i = 0; i < wordLen; i++) {

			for (int j = i; j + wordLen <= 100; j += wordLen)
			{
				resudsf++;
			}
		}

	}

	{
		std::vector<int> ValuesVector = { 5,5,4,3,3,3,3,1 };
		int inddex = s.search(ValuesVector, 1);
		inddex = s.search(ValuesVector, 10);

		ValuesVector.insert(ValuesVector.begin(), 2);
		inddex = 11;
	}

	{
		std::vector<std::vector<char>> board = {
			{'5', '3', '.', '.', '7', '.', '.', '.', '.'},
			{'6', '.', '.', '1', '9', '5', '.', '.', '.'},
			{'.', '9', '8', '.', '.', '.', '.', '6', '.'},
			{'8', '.', '.', '.', '6', '.', '.', '.', '3'},
			{'4', '.', '.', '8', '.', '3', '.', '.', '1'},
			{'7', '.', '.', '.', '2', '.', '.', '.', '6'},
			{'.', '6', '.', '.', '.', '.', '2', '8', '.'},
			{'.', '.', '.', '4', '1', '9', '.', '.', '5'},
			{'.', '.', '.', '.', '8', '.', '.', '7', '9'}
		};

		s.solveSudoku(board);

		bool resultsdfsdf = 0;

	}


	{

		char c2 = 65;
		char c = 'A';
		auto resolt = s.countAndSay(4);
		int va = 224;

	}

	{
		std::vector<int> templist = { 2,3,6,7 };
		s.combinationSum(templist, 7);
	}


	{
		std::vector<int> templist = { 10, 1, 2, 7, 6, 1, 5 };
		s.combinationSum2(templist, 8);
	}


	{
		std::vector<int> templist{ 3,4,-1,1 };
		s.firstMissingPositive(templist);

	}


	{
		std::vector<int> templist{ 0,1,0,2,1,0,1,3,2,1,2,1 };
		s.trap(templist);
	}

	{
		s.multiply("123", "456");
	}


	{
		std::vector<int> templists{ 2,3,1,1,4 };
		int rsdf = s.jump(templists);
	}

	{
		std::vector<int> templists{ 1,2,3 };
		auto sdfuiigh = s.permute(templists);
	}

	{
		std::vector<int> templists{ 1,1,2 };
		auto valuefsdf = s.permuteUnique(templists);
	}

	{
		/*	std::vector<std::string> templists{ "compilations","bewailed","horology","lactated","blindsided","swoop","foretasted","ware","abuts","stepchild","arriving","magnet","vacating","relegates","scale","melodically","proprietresses","parties","ambiguities","bootblacks","shipbuilders","umping","belittling","lefty","foremost","bifocals","moorish","temblors","edited","hint","serenest","rendezvousing","schoolmate","fertilizers","daiquiri","starr","federate","rectal","case","kielbasas","monogamous","inflectional","zapata","permitted","concessions","easters","communique","angelica","shepherdess","jaundiced","breaks","raspy","harpooned","innocence","craters","cajun","pueblos","housetop","traits","bluejacket","pete","snots","wagging","tangling","cheesecakes","constructing","balanchine","paralyzed","aftereffects","dotingly","definitions","renovations","surfboards","lifework","knacking","apprises","minimalism","skyrocketed","artworks","instrumentals","eardrums","hunching","codification","vainglory","clarendon","peters","weeknight","statistics","ay","aureomycin","lorrie","compassed","speccing","galen","concerto","rocky","derision","exonerate","sultrier","mastoids","repackage","cyclical","gowns","regionalism","supplementary","bierce","darby","memorize","songster","biplane","calibrates","decriminalizes","shack","idleness","confessions","snippy","barometer","earthing","sequence","hastiness","emitted","superintends","stockades","busywork","dvina","aggravated","furbelow","hashish","overextended","foreordain","lie","insurance","recollected","interpreted","congregate","ranks","juts","dampen","gaits","eroticism","neighborhoods","perihelion","simulations","fumigating","balkiest","semite","epicure","heavier","masterpiece","bettering","lizzie","wail","batsmen","unbolt","cudgeling","bungalow","behalves","refurnishes","pram","spoonerisms","cornered","rises","encroachments","gabon","cultivation","parsed","takeovers","stampeded","persia","devotional","doorbells","psalms","cains","copulated","archetypal","cursores","inbred","paradigmatic","thesauri","rose","stopcocks","weakness","ballsier","jagiellon","torches","hover","conservationists","brightening","dotted","rodgers","mandalay","overjoying","supervision","gonads","portage","crap","capers","posy","collateral","funny","garvey","ravenously","arias","kirghiz","elton","gambolled","highboy","kneecaps","southey","etymology","overeager","numbers","ebullience","unseemly","airbrushes","excruciating","gemstones","juiciest","muftis","shadowing","organically","plume","guppy","obscurely","clinker","confederacies","unhurried","monastic","witty","breastbones","ijsselmeer","dublin","linnaeus","dervish","bluefish","selectric","syllable","pogroms","pacesetters","anastasia","pandora","foci","bipartisan","loomed","emits","gracious","warfare","uncouples","augusts","portray","refinery","resonances","expediters","deputations","indubitably","richly","motivational","gringo","hubris","mislay","scad","lambastes","reemerged","wart","zirconium","linus","moussorgsky","swopped","sufferer","sputtered","tamed","merrimack","conglomerate","blaspheme","overcompensate","rheas","pares","ranted","prisoning","rumor","gabbles","lummox","lactated","unzipping","tirelessly","backdate","puzzling","interject","rejections","bust","centered","oxymoron","tangibles","sejong","not","tameness","consumings","prostrated","rowdyism","ardent","macabre","rustics","dodoes","warheads","wraths","bournemouth","staffers","retold","stiflings","petrifaction","larkspurs","crunching","clanks","briefest","clinches","attaching","extinguished","ryder","shiny","antiqued","gags","assessments","simulated","dialed","confesses","livelongs","dimensions","lodgings","cormorants","canaries","spineless","widening","chappaquiddick","blurry","lassa","vilyui","desertions","trinket","teamed","bidets","mods","lessors","impressiveness","subjugated","rumpuses","swamies","annotations","batiks","ratliff","waxwork","grander","junta","chutney","exalted","yawl","joke","vocational","diabetic","bullying","edit","losing","banns","doleful","precision","excreting","foals","smarten","soliciting","disturbance","soggily","gabrielle","margret","faded","pane","jerusalem","bedpan","overtaxed","brigs","honors","repackage","croissants","kirov","crummier","limeades","grandson","criers","bring","jaundicing","omnibusses","gawking","tonsillectomies","deodorizer","nosedove","commence","faulkner","adultery","shakedown","wigwag","wiper","compatible","ultra","adamant","distillation","gestates","semi","inmate","onlookers","grudgingly","recipe","chaise","dialectal","aphids","flimsier","orgasm","sobs","swellheaded","utilize","karenina","irreparably","preteen","mumble","gingersnaps","alumnus","chummiest","snobbish","crawlspaces","inappropriate","ought","continence","hydrogenate","eskimo","desolated","oceanic","evasive","sake","laziest","tramps","joyridden","acclimatized","riffraff","thanklessly","harmonizing","guinevere","demanded","capabler","syphilitics","brainteaser","creamers","upholds","stiflings","walt","luau","deafen","concretely","unhand","animations","map","limbos","tranquil","windbreakers","limoges","varying","declensions","signs","green","snowbelt","homosexual","hopping","residue","ransacked","emeritus","pathologist","brazenly","forbiddingly","alfredo","glummest","deciphered","delusive","repentant","complainants","beets","syntactics","vicissitude","incompetents","concur","canaan","rowdies","streamer","martinets","shapeliness","videodiscs","restfulness","rhea","consumed","pooching","disenfranchisement","impoverishes","behalf","unsuccessfully","complicity","ulcerating","derisive","jephthah","clearing","reputation","kansan","sledgehammer","benchmarks","escutcheon","portfolios","mandolins","marketable","megalomaniacs","kinking","bombarding","wimple","perishes","rukeyser","squatter","coddle","traditionalists","sifts","agglomerations","seasonings","brightness","spices","claimant","sofas","ambulatories","bothered","businessmen","orly","kinetic","contracted","grenadiers","flooding","dissolved","corroboration","mussed","squareness","alabamans","dandelions","labyrinthine","pot","waxwing","residential","pizza","overjoying","whelps","overlaying","elanor","tented","masterminded","balsamed","powerhouses","tramps","eisenstein","voile","repellents","beaus","coordinated","wreckers","eternities","untwists","estrangements","vitreous","embodied"};
			auto result = s.groupAnagrams(templists);*/
	}

	{
		s.myPow(2.0, 10);
	}

	{
		s.solveNQueens(4);
	}

	{
		std::vector<int> templists{ 1,2,-1,-2,2,1,-2,1,4,-5,4 };
		s.maxSubArray(templists);
	}

	{
		std::vector<int> templist{ };
		templist.insert(templist.begin(), 45);

		s.canJump(templist);
	}

	{
		std::vector<std::vector<int>> templists;
		std::vector<int> list2 = { 5,7 };

		s.insert(templists, list2);


	}

	{
		s.getPermutation(4, 4);
	}

	{

		int listNum = 5;
		std::vector<Solution::ListNode> listnodes(listNum);
		std::vector<Solution::ListNode*> listnodePtrs(listNum);
		for (int i = 0; i < listNum; i++)
		{
			listnodes[i].val = i + 1;
			listnodePtrs[i] = &listnodes[i];
		}
		for (int i = 0; i < listNum - 1; i++)
		{
			listnodePtrs[i]->next = listnodePtrs[i + 1];
		}

		s.rotateRight(listnodePtrs[0], 2);
	}

	{
		std::vector<std::vector<int>> templist = { {0,0,0},{0,1,0},{0,0,0} };
		s.uniquePathsWithObstacles(templist);
	}

	{
		std::vector<std::vector<int>> templist = { {1,3,1 }, { 1,5,1 }, { 4,2,1 } };
		s.minPathSum(templist);

	}

	{
		std::string a1 = "11";
		std::string b1 = "1";
		s.addBinary(a1, b1);
	}

	{
		s.mySqrt(4);
	}

	{
		s.minDistance("h", "r");
	}

	{
		std::vector<std::vector<int>> templist{{1,1},{1, 0}};
		s.setZeroes(templist);

	}


	return 0;
}
#include "TestGFoo.hpp"
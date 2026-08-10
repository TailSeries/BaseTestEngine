#pragma once
#include <algorithm>
#include <functional>
#include <set>
#include <stack>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <queue>
class Solution {
public:

	/*1. https://leetcode.cn/problems/two-sum/
	* 1.为什么可以一边遍历一边填充map？
	*   ：考虑A + B = target，如果map.find(target - A) 不存在，那么A进入map，后续map.find(tartget - B)的时候就可以找到map[A]了。
	*/
	std::vector<int> twoSum(std::vector<int>& nums, int target)
	{
		std::unordered_map<int, int> valuesMap;
		for (int i = 0; i < nums.size(); i++)
		{
			int tempvalue = target - nums[i];
			auto it = valuesMap.find(tempvalue);
			if (it != valuesMap.end())
			{
				return { it->second, i };
			}
			valuesMap.emplace(nums[i], i);
		}
		return {};
	}



	/*2. https://leetcode.cn/problems/two-sum/description/
	* 链表要始终注意head所在的位置，每次步进操作的时候注意node = node->next的时候，node所在的真实位置
	*/
	struct ListNode
	{
		int val;
		ListNode* next;
		ListNode() : val(0), next(nullptr) {}
		ListNode(int x) : val(x), next(nullptr) {}
		ListNode(int x, ListNode* next) : val(x), next(next) {}
	};

	ListNode* addTwoNumbers(ListNode* l1, ListNode* l2)
	{
		ListNode* Head = new ListNode(0);
		ListNode* result = Head;
		while (l1 || l2)
		{
			int value = (l1 ? l1->val : 0) + (l2 ? l2->val : 0) + Head->val;
			int high = value / 10;
			int low = value % 10;
			Head->val = low;
			Head->next = (((l1 && l1->next) || (l2 && l2->next) || high > 0) == true) ? new ListNode(high) : nullptr;
			l1 = l1 ? l1->next : nullptr;
			l2 = l2 ? l2->next : nullptr;
			Head = Head->next;
		}

		return result;
	}

	/*3. https://leetcode.cn/problems/longest-substring-without-repeating-characters/
	* 这里要注意，找到重复字符的时候，不能直接从重复位置开始重新计数，而应该从上次出现的下一个位置开始重新计数。
	* 可以直接用滑动窗口，也可以像我这样用map记录左指针下一步直接跳到的位置，但是这种方法有个问题
	* 	left = std::max(records[s[i]] + 1, left); 左指针是没有必要往回跳的，对应"cbacbacbaaabc";的情况，总的来说普通的滑动窗口更有普适性
	*/
	int lengthOfLongestSubstring(std::string s)
	{
		std::unordered_map<char, int> records;
		int maxLength = 0;
		int left = 0;
		int right = 0;
		for (int i = 0; i < s.size(); i++)
		{
			if (records.find(s[i]) != records.end())
			{
				left = std::max(records[s[i]] + 1, left);
			}
			records[s[i]] = i;
			right = i;
			int tempLength = right - left + 1;
			if (maxLength < tempLength)
			{
				maxLength = tempLength;
			}
		}
		return maxLength;
	}

	/* TODO
	* 4.  https://leetcode.cn/problems/median-of-two-sorted-arrays/description/
	* --注意返回值这里要直接/2.0
	* -- 使用归并注意归并判while (l1 < nums1.size() && (l2 == nums2.size() || nums1[l1] <= nums2[l2]))里，
	*	第一，这里我希望在一次loop里归并完毕，因此这里额外处理(l2 == nums2.size()的情况，
	*	第二，l2 == nums2.size()要放在前面进行判断，以免出现l2 == nums2.size()还要去访问nums2[l2]的情况
	*	但是这种时间复杂度应该是o(m + n)
	* --
	*/
	double findMedianSortedArrays(std::vector<int> nums1, std::vector<int> nums2)
	{
		{
			std::vector<int> temp;
			int l1 = 0;
			int l2 = 0;
			int p = 0;
			while (l1 < nums1.size() || l2 < nums2.size())
			{
				while (l1 < nums1.size() && (l2 == nums2.size() || nums1[l1] <= nums2[l2]))
				{
					temp.push_back(nums1[l1]);
					l1++;
				}

				while (l2 < nums2.size() && (l1 == nums1.size() || nums2[l2] < nums1[l1]))
				{
					temp.push_back(nums2[l2]);
					l2++;
				}
			}

			int midIndex = temp.size() / 2;
			return (temp.size() % 2 == 0) ? (temp[midIndex - 1] + temp[midIndex]) / 2.0 : (temp[midIndex]);
		}

		{
			int k = (nums1.size() + nums2.size()) / 2;

			int temp = k / 2;
			if (nums1[temp] < nums2[temp])
			{

			}



		}

	}

	/*
	 * 5. https://leetcode.cn/problems/longest-palindromic-substring/description/
	 * 动态规划
	 * 核心定义： dp[i][j] = s[i..j] 是否是回文
	 * 递推关系
	 * 一个字符串是回文，当且仅当：
	 * 1. 首尾字符相等 s[i] == s[j]
	 * 2. 去掉首尾后，内部也是回文 dp[i+1][j-1] == true
	 * 关键：dp[i][j] 依赖 dp[i+1][j-1]，所以要按子串长度从小到大填。
	 */
	std::string longestPalindrome(std::string s)
	{

		{
			if (s.size() <= 1) return s;
			int start = 0;
			int maxLen = 1;
			auto expand = [&](int l, int r)
				{
					while (l >= 0 && r < s.size() && s[l] == s[r])
					{
						int tempmaxLen = r - l + 1;
						if (tempmaxLen > maxLen)
						{
							start = l;
							maxLen = tempmaxLen;
						}
						l--;
						r++;
					}
				};

			for (int i = 0; i < s.size(); i++)
			{
				// 关键：分别尝试作为奇数中心/偶数中心
				expand(i, i);
				expand(i, i + 1);
			}
			return s.substr(start, maxLen);

		}

		{


			int n = s.size();
			std::vector<std::vector<bool>> dp(n, std::vector<bool>(n, false));
			for (int i = 0; i < n; i++) dp[i][i] = true;
			int start = 0;
			int maxLen = 1;
			for (int len = 2; len <= n; len++)
			{
				for (int i = 0; i + len - 1 < n; i++)
				{
					int j = i + len - 1;
					if (s[i] == s[j])
					{
						dp[i][j] = (len == 2 || dp[i + 1][j - 1]);
					}
					if (dp[i][j] && len > maxLen)
					{
						maxLen = len;
						start = i;
					}
				}

			}
			return s.substr(start, maxLen);
		}
	}

	/*
	 * 6. https://leetcode.cn/problems/zigzag-conversion/
	 *  关键：每行其实都是一个新字符串，按行重新构建字符串就行
	 */
	std::string convert(std::string s, int numRows)
	{
		if (numRows <= 1) return s;
		std::vector<std::string> rows(numRows);
		int row = 0;
		int dir = -1;
		for (auto c : s)
		{
			rows[row] += c;
			if (row == 0 || row == numRows - 1)
			{
				dir = -dir;
			}
			row += dir;
		}
		std::string res;
		for (auto& it : rows)
		{
			res += it;
		}
		return res;
	}


	/*
	 * 7. https://leetcode.cn/problems/reverse-integer/
	 * / 与 %的应用
	 */
	int reverse(int x)
	{
		int res = 0;
		while (x != 0)
		{
			int digit = x % 10;
			x = x / 10;
			if (res > INT_MAX / 10 || (res == INT_MAX / 10 && digit > 7)) return 0;
			if (res < INT_MIN / 10 || (res == INT_MIN / 10 && digit < -8)) return 0;
			res = res * 10 + digit;
		}
		return res;
	}

	/*
	 * 8. https://leetcode.cn/problems/string-to-integer-atoi/description/
	 */

	int myAtoi(std::string s)
	{
		int i = 0;
		int n = s.size();
		while (i < n && s[i] == ' ')
		{
			i++;
		}

		bool sign = 1;
		while (i < n && (s[i] == '-' || s[i] == '+'))
		{
			sign = s[i] == '-' ? -1 : 1;
			i++;
		}
		int res = 0;
		while (i < n && std::isdigit(s[i]))
		{
			int  digit = s[i] - '0';
			if (res > INT_MAX / 10 || (res == INT_MAX / 10 && digit > 7))
			{
				return sign == 1 ? INT_MAX : INT_MIN;
			}
			i++;
			res = res * 10 + digit;
		}
		return res;
	}

	/*
	 * 9. https://leetcode.cn/problems/palindrome-number/
	 * 转成字符串看回文
	 *
	 */
	bool isPalindrome(int x)
	{
		std::string s = std::to_string(x);
		int tailIndex = s.size() - 1;
		for (int i = 0; i < s.size(); i++, tailIndex--)
		{
			if (s[i] != s[tailIndex])
			{
				return false;
			}
		}

		{
			return x == reverse(x);
		}
	}

	/*
	 * 10. https://leetcode.cn/problems/regular-expression-matching/description/
	 */


	 /*
	  * 11. https://leetcode.cn/problems/container-with-most-water/description/
	  * 双指针，从左右两侧移动矮的那一侧
	  * area = min(h[l], h[r])(r - l), 移动r或者l 宽度一定减小，在宽度减小的前提下，移动更矮的那一侧：两种情况 更小/更大，移动更高的一侧呢，h[]只会更小。
	  * 结论反过来理解：正因为移动 r 一定更小，所以 r 可以直接排除——既然移动 r 不可能变大，何必试？，
	  * 移动 l 虽然"可能更小也可能更大"，但至少还有变大的可能，值得继续探索。
	  * 所以双指针的逻辑是：每次淘汰掉一定不会是答案的那一侧，而不是"移向更好的那侧"。
	  */
	int maxArea(std::vector<int>& height) {
		int l = 0;
		int r = height.size() - 1;
		int maxaea = std::min(height[r], height[l]) * (r - l);
		while (l < r)
		{
			int temparea = std::min(height[r], height[l]) * (r - l);
			if (temparea > maxaea)
			{
				maxaea = temparea;
			}
			if (height[l] <= height[r])
			{
				l++;
			}
			else
			{
				r--;
			}
		}
		return maxaea;
	}

	/*
	 * 14. https://leetcode.cn/problems/longest-common-prefix/
	 */
	std::string longestCommonPrefix(std::vector<std::string>& strs)
	{
		if (strs.size() == 0) return "";
		if (strs.size() == 1) return strs[0];
		std::string& base = strs[0];
		int len = 1;
		std::string teststr = "";
		while (len <= base.size())
		{
			teststr = base.substr(0, len);
			bool isPrefix = true;
			for (int i = 1; i < strs.size(); i++)
			{
				if (!strs[i].starts_with(teststr))
				{
					isPrefix = false;
					break;
				}
			}
			if (isPrefix)
			{
				len++;
			}
			else
			{
				len--;
				break;
			}
		}
		teststr = base.substr(0, len);
		return teststr;
	}

	/*
	 * 15. https://leetcode.cn/problems/3sum/
	 * 先固定一个数 nums[i]，问题变成在剩余数组里找两数之和等于 -nums[i]，这就回到了双指针.
	 * 优化计算，先排序：
	 * - s < 0：和太小，需要更大的数 → l++（左侧往右走，值变大）
		- s > 0：和太大，需要更小的数 → r--（右侧往左走，值变小）
		- s == 0：找到答案，记录，然后 l++ r-- 继续找下一组
		这里有个边界条件要小心，			if (i > 0 && nums[i] == nums[i - 1]) continue; // 去掉重复项
	 */
	std::vector<std::vector<int>> threeSum(std::vector<int>& nums)
	{
		std::sort(nums.begin(), nums.end());
		std::vector<std::vector<int>> results;
		int n = nums.size();
		for (int i = 0; i < n - 2; i++)
		{
			if (nums[i] > 0) break;
			if (i > 0 && nums[i] == nums[i - 1]) continue; // 去掉重复计算

			int l = i + 1;
			int r = n - 1;
			while (l < r)
			{
				int s = nums[i] + nums[l] + nums[r];
				if (s == 0)
				{
					results.push_back({ nums[i], nums[l],nums[r] });
					while (l < r && nums[l] == nums[l + 1]) l++;
					while (l < r && nums[r] == nums[r - 1]) r--;
					// 为什么同时操作l r ? 现在i是固定的，如果l/r固定，那么下一个数要成立就必须是重复数字，不算数，那不如直接避免计算
					l++;
					r--;
				}
				else if (s < 0)
				{
					l++;
				}
				else
				{
					r--;
				}
			}
		}
		return results;
	}


	/*
	 * 16. https://leetcode.cn/problems/3sum-closest/description/
	 * 优化计算，先排序：
	 * - s < target：和太小，需要更大的数 → l++（左侧往右走，值变大）
		- s > target：和太大，需要更小的数 → r--（右侧往左走，值变小）
		- s == target：找到答案
	 */

	int threeSumClosest(std::vector<int>& nums, int target)
	{
		std::sort(nums.begin(), nums.end());
		int n = nums.size();
		int closest = nums[0] + nums[1] + nums[2];
		for (int i = 0; i < n - 2; i++)
		{
			int r = n - 1;
			int l = i + 1;
			while (l < r)
			{
				int temp = nums[i] + nums[r] + nums[l];
				if (std::abs(temp - target) < abs(closest - target))
				{
					closest = temp;
				}

				//
				if (temp < target)
				{
					// 希望下一个temp更大
					l++;
				}
				else if (temp > target)
				{
					// 希望下一个temp更小
					r--;
				}
				else
				{
					return temp;
				}
			}
		}
		return closest;
	}

	/*
	 * 17. https://leetcode.cn/problems/letter-combinations-of-a-phone-number/
	 * 一个不定长序列的组合问题
	 * （先写几项找规律，意识到可以递归）
	 */

	std::vector<std::string> letterCombinations(std::string digits)
	{
		std::vector<std::string> marks = { "","", "abc", "def", "ghi", "jkl", "mno", "pqrs", "tuv", "wxyz" };
		std::vector<std::string> results;
		std::function<void(std::string, int)> func = [&](std::string resultStr, int digitIndex)
			{
				int markIndex = digits[digitIndex] - '0';
				const std::string& markStr = marks[markIndex];
				int msize = markStr.size();
				digitIndex++;
				for (int i = 0; i < msize; i++)
				{
					char c = markStr[i];
					std::string tempStr = resultStr + c;
					if (digitIndex < digits.size())
					{
						func(tempStr, digitIndex);
					}
					else
					{
						results.push_back(tempStr);
					}
				}
			};


		func("", 0);
		return results;
	}

	/*
	 * 18. https://leetcode.cn/problems/4sum/
	 * 排序，双指针，三数之和上加一层, 最核心的里面仍然是快速计算两个数相加怎么最快匹配到对应target
	 * 边界条件关键在于去重思路
	 */
	std::vector<std::vector<int>> fourSum(std::vector<int>& nums, int target)
	{
		std::sort(nums.begin(), nums.end());
		int n = nums.size();
		std::vector<std::vector<int>> result;
		for (int i = 0; i < n - 3; i++)
		{
			if (i > 0 && nums[i] == nums[i - 1]) continue;// 下层要计算的是3数之和，这里如果i是一样的，那么没必要继续计算
			for (int j = i + 1; j < n - 2; j++)
			{
				if (j > i + 1 && nums[j] == nums[j - 1]) continue; // 下层要计算的是两数之和，如果j是一样的，这里没必要继续计算
				int l = j + 1;
				int r = n - 1;
				while (l < r)
				{
					int64_t sum = (int64_t)(nums[i]) + nums[j] + nums[l] + nums[r];
					if (sum == target)
					{
						result.push_back({ nums[i], nums[j], nums[l], nums[r] });
						while (l < r && nums[l] == nums[l + 1]) l++;
						while (l < r && nums[r] == nums[r - 1]) r--;
						l++;
						r--;
					}
					else if (sum < target)
					{
						l++;
					}
					else
					{
						r--;
					}
				}
			}
		}
		return result;
	}


	/*
	 * 19. https://leetcode.cn/problems/remove-nth-node-from-end-of-list/description/
	 * 考虑一次遍历，应该只能在退栈的时候去做删除了
	 * **用递归的时候注意传入的是指针的引用, 这里代码很简洁，但是看着麻烦，而且处理大规模数据有栈溢出风险
	 * 用stack更加直观, 这里直接用vector，方法更多
	 */
	ListNode* removeNthFromEnd(ListNode* head, int n)
	{
		if (n == 0) return head;
		{
			std::vector<ListNode*> values;
			ListNode* Node = head;
			while (Node)
			{
				values.push_back(Node);
				Node = Node->next;
			}
			if (n > values.size()) return head;

			ListNode* RemoveNode = values[values.size() - n];
			ListNode* PreNode = n == values.size() ? nullptr : values[values.size() - n - 1];
			if (PreNode)
			{
				PreNode->next = RemoveNode->next;
			}
			else
			{
				// PreNode 不存在说明
				head = head->next;
			}
			return head;
		}

		{
			std::function<void(ListNode*&, int&)> findfunc = [&](ListNode*& Node, int& n)
				{
					if (Node)
					{
						findfunc(Node->next, n);
						n--;
						if (n == 0)
						{
							Node = Node->next;
						}
					}
				};
			findfunc(head, n);
			return head;
		}
	}

	/*
	 * 20. https://leetcode.cn/problems/valid-parentheses/description/
	 * 栈操作
	 * *简洁版，利用了map反指，可以省略数字映射
	 */

	bool isValid(std::string s)
	{
		{
			std::stack<char> st;
			std::unordered_map<char, char> pairs = {
				{')', '('},
				{']', '['},
				{'}', '{'}
			};

			for (char ch : s) {
				if (pairs.count(ch)) {
					if (st.empty() || st.top() != pairs[ch])
						return false;
					st.pop();
				}
				else {
					st.push(ch);
				}
			}

			return st.empty();
		}

		std::map<char, int> maptables{ {'(',1},{'{',2}, {'[',3}, {')',-1},{'}',-2}, {']',-3} };
		std::stack<char> values;
		for (int i = 0; i < s.size(); i++)
		{
			char curChar = s[i];
			int curV = maptables[curChar];
			if (values.empty())
			{
				if (curV > 0)
				{
					values.push(curChar);
				}
				else
				{
					return false;
				}
			}
			else
			{

				char preChar = values.top();
				int prev = maptables[preChar];
				if (curV < 0)
				{
					if (prev == -curV)
					{
						values.pop();
					}
					else
					{
						return false;
					}
				}
				else
				{
					values.push(curChar);
				}
			}
		}

		return values.empty();

	}

	/*
	 * 21. https://leetcode.cn/problems/merge-two-sorted-lists/
	 * 直接归并
	 */
	ListNode* mergeTwoLists(ListNode* list1, ListNode* list2)
	{
		ListNode* Root = new ListNode();
		ListNode* temp = Root;

		while (list1 || list2)
		{

			if (list1 == nullptr)
			{
				Root->next = list2;
				list2 = list2->next;
			}
			else if (list2 == nullptr)
			{
				Root->next = list1;
				list1 = list1->next;
			}
			else if (list1->val < list2->val)
			{
				Root->next = list1;
				list1 = list1->next;
			}
			else
			{
				Root->next = list2;
				list2 = list2->next;
			}
			Root = Root->next;
		}

		return temp->next;
	}

	/*
	 * 22. https://leetcode.cn/problems/generate-parentheses/description/
	 * 还是应该用栈， 栈空的时候只允许
	 * 1.先找出全部的，然后判断正确性，就超时了,不想超时，关键就是思考在循环过程中的剪枝条件，思考剪枝两种思路：找出全部合法的条件/找出全部非法的条件
	 * 思路：回溯
		维护两个计数器：
		- open：已放置的左括号数
		- close：已放置的右括号数

		剪枝条件：
		- open < n 时可放 (
		- close < open 时可放 )（保证合法性）
	 *
	*/
	std::vector<std::string> generateParenthesis(int n)
	{

		{
			std::vector<std::string> FinalResults;
			std::function<void(int, int, std::string)> backtrack = [&](int l, int r, std::string result)
				{
					if (result.size() == n * 2)
					{
						FinalResults.push_back(result);
					}
					else
					{
						/*
						 * 原本的情况是 backtrack("(");backtrack(")");
						 * 现在进行剪枝处理
						 */

						if (l < n)
						{
							// 不能在外面l++，会影响到下一个分支的r < 1, 下一个分支处理r < l的时候，result还没变
							backtrack(l + 1, r, result + "(");
						}

						if (r < l)
						{
							backtrack(l, r + 1, result + ")");
						}

					}
				};

			backtrack(0, 0, "");
			return FinalResults;

		}


		n = n * 2;
		std::vector<std::string> results;
		std::function<void(int, std::string)> valuefunc = [&](int n, std::string lastStr)
			{
				n--;
				std::string newStr1 = lastStr + "(";
				std::string newStr2 = lastStr + ")";

				if (n == 0)
				{
					results.push_back(newStr1);
					results.push_back(newStr2);
				}
				else
				{
					valuefunc(n, newStr1);
					valuefunc(n, newStr2);
				}
			};

		valuefunc(n, "");
		auto IsStringValid = [&](std::string s)->bool
			{
				std::stack<char> st;
				std::unordered_map<char, char> pairs = {
					{')', '('}
				};

				for (char ch : s) {
					if (pairs.count(ch)) {
						if (st.empty() || st.top() != pairs[ch])
							return false;
						st.pop();
					}
					else {
						st.push(ch);
					}
				}

				return st.empty();
			};

		for (auto it = results.begin(); it != results.end();)
		{
			if (!IsStringValid(*it))
			{
				it = results.erase(it);
			}
			else
			{
				++it;
			}
		}

		return results;

	}

	/*
	 *23. https://leetcode.cn/problems/merge-k-sorted-lists/description/
	 * 顺序合并就行
	 */

	ListNode* mergeKLists(std::vector<ListNode*>& lists)
	{

		ListNode* Root = new ListNode(-1);
		ListNode* Result = Root;
		while (lists.size() > 0)
		{
			int minIndex = 0;
			for (int i = 0; i < lists.size(); i++)
			{
				/**
				 *注意这里可能是存在null的 最小值 》 有值 》 null
				 */
				if (!lists[i]) continue;

				if (!lists[minIndex] || lists[minIndex]->val > lists[i]->val)
				{
					minIndex = i;
				}
			}

			if (!lists[minIndex])
			{
				/*
				 * 最小值 》 有值 》 null，如果 !lists[minIndex]
				 */
				break;
			}

			Root->next = lists[minIndex];
			Root = Root->next;
			lists[minIndex] = lists[minIndex]->next;
		}


		return Result->next;
	}


	/*
	 * 24. https://leetcode.cn/problems/swap-nodes-in-pairs/description/
	 * 放一个数组里面，两两交换数组元素，然后重新指定next
	 * 直接记录 abc 顺序遍历交换
	 * 递归栈，这里return second 是一个设计精华啊
	 */

	ListNode* swapPairs(ListNode* head)
	{

		{
			// 递归栈，并且是在退栈的时候去做 这样的话，避免了我需要构造一个pre的node出来, zhege
			std::function<ListNode* (ListNode*)> stacksolve = [&](ListNode* Node)->ListNode*
				{
					if (!Node || !Node->next) return Node;
					ListNode* first = Node;
					ListNode* Second = Node->next;
					ListNode* Nexthead = stacksolve(Second->next);
					// 出栈之后 返回这两个的新的头
					Second->next = first;
					first->next = Nexthead;
					return Second;
				};
			stacksolve(head);
		}


		auto ExchangeNode = [&](ListNode* pre, ListNode* node1, ListNode* node2)
			{
				if (!node1 || !node2) return;
				node1->next = node2->next;
				pre->next = node2;
				node2->next = node1;

			};

		ListNode* preNode = new ListNode(-1);
		ListNode* rootNode = preNode;
		preNode->next = head;
		ListNode* CurNode = preNode;
		ListNode* a = nullptr;
		ListNode* b = nullptr;
		ListNode* c = nullptr;
		while (CurNode)
		{
			a = CurNode;
			b = CurNode->next;
			if (b)
			{
				c = b->next;
				ExchangeNode(a, b, c);
				CurNode = b->next;
			}
			CurNode = b;
		}
		return rootNode->next;
	}

	/*
	 * 25. https://leetcode.cn/problems/reverse-nodes-in-k-group/description/
	 * 关键在于间隔一段长度更新
	 */

	ListNode* reverseKGroup(ListNode* head, int k) {

		std::vector<ListNode*> tempStacks;
		ListNode* ResultNode = new ListNode(-1);
		ResultNode->next = head;

		ListNode* Temp = ResultNode;
		bool shouldPush = true;
		while (ResultNode)
		{
			if (shouldPush)
			{
				tempStacks.push_back(ResultNode);
			}


			ResultNode = ResultNode->next;

			if (tempStacks.size() == k + 1)
			{
				// 满了
				shouldPush = false;
			}

			if (!shouldPush)
			{
				/*
				 * 翻转0 1 2 3 4 | 5 》》 0 | 4 3 2 1 | 5
				 * 重新建立 0 - 4  1 - 5
				 */


				for (int i = tempStacks.size() - 1; i > 1; i--)
				{
					tempStacks[i]->next = tempStacks[i - 1];
				}

				tempStacks[0]->next = tempStacks[tempStacks.size() - 1];

				//? tempstack[1]
				ListNode* newTopNode = tempStacks[1];
				newTopNode->next = ResultNode;
				tempStacks.clear();
				tempStacks.push_back(newTopNode);
				shouldPush = true;
			}
		}

		return Temp->next;

	}


	/*
	 *26. https://leetcode.cn/problems/remove-duplicates-from-sorted-array/
	 * 反过来思考，不是要删除，而是要把不同值往前进行复制，双指针，一个指向可复制位置，一个指向当前遍历到的位置.
	 *
	 */
	int removeDuplicates(std::vector<int>& nums)
	{
		if (nums.empty()) return 0;
		int last = *nums.end();
		int i = 1;
		int k = 1;
		for (; i < nums.size(); i++)
		{
			if (nums[i] == nums[i - 1]) // k 始终落后于i，安全
			{
				continue;
			}

			nums[k] = nums[i];
			k++;
		}

		return k;
	}

	/*
	* 27. https://leetcode.cn/problems/remove-element/description/
	* 双指针，一个指向当前操作元素，另一个指向末尾可以被交换的元素位置
	*/

	int removeElement(std::vector<int>& nums, int val)
	{
		int k = nums.size() - 1;
		for (int i = 0; i < nums.size(); i++)
		{
			while (k > i && nums[k] == val)
			{
				k--;
				// k 最多到i
			}

			if (nums[i] == val && i <= k)
			{
				int temp = nums[i];
				nums[i] = nums[k];
				nums[k] = nums[i];
				k--;
			}
		}
		return k + 1;
	}


	/*
	 * 28. https://leetcode.cn/problems/find-the-index-of-the-first-occurrence-in-a-string/description/
	 * 双指针，一个指向主字符串字符，另一个指向待匹配的字符串
	 * 不能想当然的剪枝，不好证明覆盖所有情况
	 */

	int strStr(std::string haystack, std::string needle)
	{
		int Index = 0;
		int ChildIndex = 0;
		while (Index <= int(haystack.size() - needle.size()))
		{
			if (haystack[Index] == needle[0])
			{
				bool Finded = true;
				for (int i = 1; i < needle.size(); i++)
				{
					if (haystack[Index + i] != needle[i])
					{
						Finded = false;
						// Index = Index + i; 不能想当然在这里剪枝 "mississippi", "issip"
						break;
					}
				}
				if (Finded)
				{
					return Index;
				}
			}
			Index++;
		}

		return -1;

	}


	/*
	 * 29. https://leetcode.cn/problems/divide-two-integers/
	 * 正常思路看看 a 能减掉多少次b就行了，但这里有个一种很巧妙的加速思路，使用总数 - 每次翻倍的思路，快速逼近，范围 [-2^31, 2^31 - 1]
	 */

	int divide(int dividend, int divisor) {
		// [-2^31, 2^31 - 1]，仔细想想，这里是两个整数除法，下面就是唯一的可能的越界情况
		if (dividend == INT_MIN && divisor == -1) return INT_MAX;


		// 统一换成负数处理 （负数范围比正数大 1，避免 abs(INT_MIN) 溢出）
		bool negative = (dividend > 0) != (divisor > 0);
		long a = -std::abs((long)dividend);
		long b = -std::abs((long)divisor);

		long result = 0;
		while (a <= b) // a b 都是负数的情况下，这表示 b还能被减
		{
			long temp = b;
			long multiple = 1;
			while (a <= temp + temp)
			{
				//这里其实是一个加速结构： 以两倍的两倍翻，看看a最终可以减掉多少个b
				temp = temp + temp;
				multiple = multiple + multiple;
			}
			//剩余值再重新从b的一倍开始尝试
			a -= temp;
			result += multiple;
		}

		return negative ? -result : result;
	}

	/*
	 *30. https://leetcode.cn/problems/substring-with-concatenation-of-all-words/description/
	 * words 放进一个先收集放进一个set里，然后找出所有可能的位置，再把可能的位置进行筛选。如果剪枝的话,这种暴力法剪枝思考很绕
	 *
	 */
	std::vector<int> findSubstring(std::string s, std::vector<std::string>& words)
	{
		std::vector<int> result;
		if (words.empty()) return result;

		if (words[0].empty())
		{
			if (s.empty())
			{
				return { 0 };
			}
			else
			{
				return result;
			}
		}

		int strip = words[0].size();

		if (s.empty() || s.size() < strip * words.size()) return result;

		std::map<std::string, int> Records;
		for (auto& it : words)
		{
			if (Records.find(it) == Records.end())
			{
				Records[it] = 1;
			}
			else
			{
				Records[it]++;
			}
		}


		// cong 0
		std::function<bool(const std::string& tempstr, int& index)> markRecords = [&](const std::string& tempstr, int index)->bool
			{
				if (index >= words.size()) return true;
				std::string tocheckstr = tempstr.substr(index * strip, strip);

				auto it = Records.find(tocheckstr);

				if (it != Records.end() && it->second > 0)
				{
					it->second--;
					index++;
					bool couldRecusive = markRecords(tempstr, index);
					// 递归查找，退栈后把元素数量还回去， 因为我们永远是查一个进递推，所以这里还回去也不会影响下一个
					it->second++;
					return couldRecusive;
				}
				else
				{
					return false;
				}
			};


		for (int i = 0; i < s.size() - strip * words.size() + 1; i += 1)
		{
			std::string tempStr = s.substr(i, strip * words.size());
			int index = 0; // 实际上代表了连续有几个元素是合格的
			bool IsOk = markRecords(tempStr, index);
			if (IsOk)
			{
				result.push_back(i);
			}
		}




		return result;
	}


	/*
	 * 31. https://leetcode.cn/problems/next-permutation/
	 * 费事的地方在于原地
	 * 1. 首先，为什么要去找最后一个小高点位置的前一个点:如果i前面相同，固定i左边不动，i右边严格升序，那么这就是i前面固定后的最小排列。i右边严格降序那就是最大排列。
	 *	那么对于i位置的数 a[i]b而言，这是不变动i位置时的最大排列。
	 *	2.这个排列a[i]b的下一个排列就是，从b里面找一个大于i的最小的数，然后让右侧严格升序
	 * 
	 */
	void nextPermutation(std::vector<int>& nums) 
	{
		int n = nums.size();
		int i = n - 2;
		//最终i就是小高点的前一个点
		while (i >= 0 && nums[i] >= nums[i + 1]) 
		{
			i--;
		}
		// i 右边严格降序，那现在就i固定，左边不动的情况下，最大排列。 找完之后 至少 nums[i+1] > nums[i]

		if (i >= 0)
		{
			// 找右边第一个比i大的，（因为右边严格降序，所以也就是比i大的最小数） 但最多j  = i + 1
			int j = n - 1;
			while (nums[j] <= nums[i])
			{
				j--;
			}
			std::swap(nums[i], nums[j]);
		}
		// 因为右边本来是严格降序，被替换的位置又是比i大的最小数，那么右边直接翻转顺序就行。
		std::reverse(nums.begin() + i + 1, nums.end());
	}


	/*
	 *32. https://leetcode.cn/problems/longest-valid-parentheses/description/
	 */
	int longestValidParentheses(std::string s) 
	{

	}
};

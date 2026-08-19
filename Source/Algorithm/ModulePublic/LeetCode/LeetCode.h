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
	 * 栈方法：1. 要的是长度 → 得靠下标相减 → 栈存下标。2. 相减需要左边界 → 左边界就是「最近的断点」。3. 开头没有断点 → 预置 -1 当哨兵。
	 * 动态规划法：定义dp与dp递推条件很难一次性想到
	 */
	int longestValidParentheses(std::string s)
	{
		{
			//先预存一个不合法的边界，实际上字符串长度 = 最后合法点-不合法边界基准
			int res = 0;
			std::stack<int> st;
			st.push(-1);
			for (uint16_t i = 0; i < s.size(); i++)
			{
				if (s[i] == '(')
				{
					st.push(i);
				}
				else
				{
					st.pop();
					if (st.empty())
					{
						// 一开始放入了一个) 这里栈空 就表示新的这个）是多余的，所以应该称为新基准
						// 栈空，更新新的基准
						st.push(i);
					}
					else
					{
						res = std::max(res, i - st.top());
					}
				}
			}
			return res;
		}

		{
			//dp[i] i位置处的往前数的连续子串长度 
			//1. 如果 s[i] == '('，它不可能是任何有效串的结尾（有效串必须以 ) 收尾），所以 dp[i] = 0。
			//2. 只有 s[i] == ')' 时才需要计算。s[i-1] == '(' 那么正好 s[i -1, i]=(), dp[i] = dp[i-2] + 2; 
			//		情况二：s[i-1] == ')'必须跳过 s[i-1] 结尾的那一整段有效串，去看更前面的字符 j = i - dp[i-1] - 1 只有当 s[j] == '(' 时，s[i] 才能和它配上 dp[i] = dp[i-1] + 2 + dp[j-1]
			int res = 0;
			int n = s.size();
			std::vector<int> dp(n, 0);
			for (int i = 1; i < n; i++)
			{
				if (s[i] == ')')
				{
					if (s[i - 1] == '(')
					{
						dp[i] = (i >= 2 ? dp[i - 2] : 0) + 2;
					}
					else
					{
						if (dp[i - 1] > 0)
						{
							// 前一个成对
							int j = i - dp[i - 1] - 1;
							if (j >= 0 && s[j] == '(')
							{
								//  (j >= 1 ? dp[j - 1] : 0); 把前面一个不连续带给干掉了，这里就要续上。也就是说
								dp[i] = dp[i - 1] + 2 + (j >= 1 ? dp[j - 1] : 0);
							}

						}
						else
						{
							// i - 1是 ) 不成对， 现在i还是）同样不成对，略过
						}

					}
				}
			}



		}

	}

	/*
	 * 33. https://leetcode.cn/problems/search-in-rotated-sorted-array/description/
	 * 无重复单调数组被旋转过，于是有个关键性质，如果切一半，那至少其中一半是单调的
	 */

	int search(std::vector<int>& nums, int target)
	{

		{
			int left = 0;
			int right = nums.size() - 1;
			while (left <= right)
			{
				int mid = (right + left) / 2;
				if (nums[mid] == target) return mid;

				if (nums[left] <= nums[mid])// [left ~ mid] 单调
				{
					if (nums[left] <= target && target <= nums[mid])
					{
						// target 在左半有序区间里面
						right = mid;
					}
					else
					{
						left = mid + 1;
					}
				}
				else
				{
					// 这说明 [mid ~ right] 一定单调
					if (nums[mid] <= target && target <= nums[right])
					{
						// target 在右半边单调区间里
						left = mid;
					}
					else
					{
						right = mid - 1;
					}
				}

			}
			return -1;

		}



		// 我这里直接二分找到了中间分割点，但是上面有更直接的做法
		if (nums.size() == 0) return -1;

		int n = nums.size();
		int r = n - 1;
		int l = 0;


		std::function<int(int)> binarysearch = [&](int index)->int
			{
				if (nums[index] < nums[l] && nums[index] < nums[r])
				{
					//index 在  右半
					r = index;
					index = (l + r) / 2;

					return binarysearch(index);

				}
				else if (nums[index] > nums[l] && nums[index] > nums[r])
				{
					//index 在 左半
					l = index;
					index = (l + r) / 2;
					return binarysearch(index);

				}
				else
				{
					if (nums[index] == nums[l]) return l;
					return r;
				}
			};

		int trypoint = (n - 1) / 2;
		int point = binarysearch(trypoint);

		std::function<int(int, int)> findfunc = [&](int left, int right)->int
			{
				if (left == right)
				{
					return target == nums[left] ? left : -1;
				}
				int mid = (right + left) / 2;
				if (target > nums[mid])
				{
					return findfunc(mid + 1, right);
				}
				else if (target < nums[mid])
				{
					return findfunc(left, mid - 1);
				}
				else
				{
					return mid;
				}
			};
		if (target <= nums[point] && target >= nums[0])
		{
			// 左半边
			return findfunc(0, point);
		}
		else
		{
			return findfunc(point + 1, n - 1);
		}
	}


	/*
	 * 34. https://leetcode.cn/problems/find-first-and-last-position-of-element-in-sorted-array/description/
	 * 非递减，log(n)
	 * 非递减，那么这个值的位置一定是相邻的,我们注意用二分找边界
	 *
	 */
	std::vector<int> searchRange(std::vector<int>& nums, int target)
	{

		{
			auto lowerBound = [&](int x)->int
				{
					// 第一个 >= x 的下标（找不到返回 nums.size()）
					int left = 0;
					int right = nums.size();
					while (left < right)
					{
						int mid = (left + right) / 2;
						if (nums[mid] < x)
						{
							left = mid + 1;
						}
						else
						{
							right = mid;
						}
					}
					return left;

				};
			int left = lowerBound(target);
			// target 不存在：越界，或该位置不是 target
			if (left == (int)nums.size() || nums[left] != target)
				return { -1, -1 };

			int right = lowerBound(target + 1) - 1;  // 第一个 > target 的前一格
			return { left, right };
		}



		int left = 0;
		int right = nums.size() - 1;
		while (left <= right)
		{
			int mid = (left + right) / 2;
			if (target == nums[mid])
			{
				// 现在找到了，而且第一次找到的时候 left   right 包含了mid的左右界在里面。直接缩left 和 right
				while (nums[left] != target)
				{
					left++;
				}
				while (nums[right] != target)
				{
					right--;
				}
				return { left, right };
			}
			if (target < nums[mid])
			{
				right = mid - 1;
			}
			else
			{
				left = mid + 1;
			}


		}

		return { -1, -1 };

	}

	/*
	 *35. https://leetcode.cn/problems/search-insert-position/description/
	 *升序：找第一个 >= target的位置
	 *降序：找最后一个 >= target的位置
	 */

	int searchInsert(std::vector<int>& nums, int target)
	{
		int l = 0;
		int r = nums.size();
		bool isUp = nums[l] <= nums[r - 1];
		// 处理升序
		auto lowerbound = [&](int x)->int
			{
				int left = 0;
				int right = nums.size();
				while (left < right)
				{
					int mid = (left + right) / 2;
					if (target <= nums[mid])
					{
						right = mid;
					}
					else
					{
						left = mid + 1;
					}
				}
				return left;
			};

		// 最后一个比x大的下一个
		auto func2 = [&](int x)->int
			{
				int left = 0;
				int right = nums.size();
				while (left < right)
				{
					int mid = (left + right) / 2;
					if (target <= nums[mid])
					{
						left = mid + 1;
					}
					else
					{
						right = mid;
					}

				}
				return left;
			};

		if (isUp)
		{
			return lowerbound(target);
		}
		else
		{
			return func2(target);
		}


	}


	/*
	 * 36. https://leetcode.cn/problems/valid-sudoku/de
	 * scription/
	 * 行不重复，列不重复，小九格不重复. 注意小九宫格计算行数
	 */
	bool isValidSudoku(std::vector<std::vector<char>>& board)
	{
		std::map<int, std::set<char>> rowCheck;
		auto isrowvalid = [&](int row)->bool
			{
				std::vector<char>& rowValues = board[row];
				std::set<char> rownumbers;
				for (auto& it : rowValues)
				{
					if (it == '.')
					{
						continue;
					}

					auto iter = rownumbers.find(it);
					if (iter != rownumbers.end())
					{
						return false;
					}
					else
					{
						rownumbers.emplace(it);
					}

				}

				return true;
			};
		auto isClomuvalid = [&](int clo)->bool
			{
				std::set<char> colvalues;
				for (int row = 0; row < 9; row++)
				{
					std::vector<char>& rowValues = board[row];
					char clovalue = rowValues[clo];
					if (clovalue == '.')
					{
						continue;
					}
					auto iter = colvalues.find(clovalue);
					if (iter != colvalues.end())
					{
						return false;
					}
					else
					{
						colvalues.emplace(clovalue);
					}
				}
				return true;
			};
		auto isLittleboardValid = [&](int litboardIdx)->bool
			{
				int startj = ((int)(litboardIdx / 3)) * 3;
				int endj = startj + 3;// 左开右闭

				int starti = (litboardIdx % 3) * 3;
				int endi = starti + 3;// 左开右闭
				std::set<char> temp;
				for (int row = startj; row < endj; row++)
				{
					std::vector<char>& currowvalues = board[row];
					for (int col = starti; col < endi; col++)
					{
						if (currowvalues[col] == '.')
						{
							continue;
						}
						auto iter = temp.find(currowvalues[col]);
						if (iter != temp.end())
						{
							return false;
						}
						else
						{
							temp.emplace(currowvalues[col]);
						}
					}
				}
				return true;
			};

		for (int i = 0; i < 9; i++)
		{
			bool result = isrowvalid(i) && isClomuvalid(i) && isLittleboardValid(i);
			if (!result)
			{
				return false;
			}
		}
		return true;

	}


	/*
	 * 37. https://leetcode.cn/problems/sudoku-solver/description/
	 * 行不重复，列不重复，小九格不重复.
	 * 注意到数独答案唯一。但实际上没有取巧方案，不能通过排除法去做，一个空格能填的数字，取决于其它空格怎么填。这是典型的"试错 + 撤销"问题：本质是一棵搜索树的深度优先遍历（DFS）
	 * 最直接的写法：每次要判断"数字 d 能不能填在 (r,c)"时，就去扫描这一行 9 个格、这一列 9 个格、这个宫 9 个格，看有没有重复。这能work，但每次判断都要遍历约 27 个格子。回溯会进行成千上万次这样的判断，很浪费。关键优化就在这里。
	 * 核心优化：位掩码（bitmask）  数字 1 用第 0 位，数字 2 用第 1 位，……数字 9 用第 8 位
	 */
	void solveSudoku(std::vector<std::vector<char>>& board)
	{
		/*

开三个数组：
int rows[9];   // rows[r]  = 第 r 行用了哪些数字
int cols[9];   // cols[c]  = 第 c 列用了哪些数字
int boxes[9];  // boxes[b] = 第 b 宫用了哪些数字

三个 O(1) 操作

┌───────────────────────┬──────────────────────────────────────┬───────────────────────────────────────┐
│         操作          │                 写法                 │                 含义                  │
├───────────────────────┼──────────────────────────────────────┼───────────────────────────────────────┤
│ 把数字 d（0~8）转成位   │ bit = 1 << d                         │ 第 d 位是 1，其余是 0                 │
├───────────────────────┼──────────────────────────────────────┼───────────────────────────────────────┤
│ 判断能否填             │ (rows[r] | cols[c] | boxes[b]) & bit │ 三处任一已用该数字，结果非 0 → 不能填 │
├───────────────────────┼──────────────────────────────────────┼───────────────────────────────────────┤
│ 填入（占位）            │ rows[r] |= bit; （列、宫同理）        │ 把那一位置 1                          │
├───────────────────────┼──────────────────────────────────────┼───────────────────────────────────────┤
│ 撤销（清位）            │ rows[r] ^= bit;                     │ 异或把那一位清 0                      │
└───────────────────────┴──────────────────────────────────────┴───────────────────────────────────────┘

▎ rows[r] | cols[c] | boxes[b] 一次运算就得到了"这个格子所有被禁止的数字"，判断从"扫 27 个格"变成"1 次位运算"。这就是快的原因。
		 */
		int rows[9] = { 0 };
		int cols[9] = { 0 };
		int boxes[9] = { 0 };
		std::vector<std::pair<int, int>> empties;
		std::function<bool(int)> backtrack = [&](int idx)->bool
			{
				if (idx == empties.size()) return true;
				auto [r, c] = empties[idx];
				int b = r / 3 * 3 + c / 3;
				int used = rows[r] | cols[c] | boxes[b];
				for (int i = 1; i <= 9; i++)
				{
					int tempbit = 1 << i;
					if (tempbit & used) continue; //数字已经用过
					used |= tempbit;
					rows[r] |= tempbit;
					cols[c] |= tempbit;
					boxes[b] |= tempbit;
					board[r][c] = '0' + i;
					// 递归下一个空格
					bool nextresult = backtrack(idx + 1);
					if (nextresult)
					{
						return true;
					}
					else
					{
						rows[r] ^= tempbit;
						cols[c] ^= tempbit;
						boxes[b] ^= tempbit;
					}
				}
				board[r][c] = '.';   // 9 个数字都失败 → 复原此格，向上层报告失败
				return false;
			};



		for (int r = 0; r < 9; r++)
		{
			for (int c = 0; c < 9; c++)
			{
				if (board[r][c] == '.')
				{
					empties.emplace_back(r, c);
				}
				else
				{
					int bit = 1 << (board[r][c] - '0');
					rows[r] |= bit;
					cols[c] |= bit;
					boxes[r / 3 * 3 + c / 3] |= bit;
				}
			}
		}

		bool result = backtrack(0);
	}


	/*
	 * 38. https://leetcode.cn/problems/count-and-say/description/
	 * 题目很费解，但正常递归做法
	 */
	std::string countAndSay(int n) {
		if (n == 1) return "1";

		std::string values = countAndSay(n - 1);
		std::string temps = "";
		char value = '.';
		int valueNum = 0;
		for (int i = 0; i < values.size(); i++)
		{
			if (i == 0)
			{
				value = values[0];
				valueNum = 1;
			}
			else
			{
				char tempValue = values[i];
				if (tempValue == value)
				{
					valueNum++;
				}
				else
				{
					temps += std::to_string(valueNum);
					temps += value;
					valueNum = 1;
					value = tempValue;
				}
			}
		}
		if (valueNum != 0)
		{
			temps += std::to_string(valueNum);
			temps += value;
			valueNum = 0;
		}


		return temps;

	}

	/*
	 * 39. https://leetcode.cn/problems/combination-sum/description/
	 * 除法和%想因式分解是行不通的，比如 7 = 2 + 2 + 3
	 * 如此一来只能回溯去减了，减到0证明这种组合可以，非0证明这种组合失败，如果数组顺序非负可以直接负数就返回false
	 * 将数据看成每一层选一个数
	 */
	std::vector<std::vector<int>> combinationSum(std::vector<int>& candidates, int target)
	{
		std::vector<std::vector<int>> Results;
		{
			std::sort(candidates.begin(), candidates.end());

			std::vector<int> tempList;
			// 去重，每一层顶多再需要把自己放进来
			std::function<void(int, int)> backTrack = [&](int startidx, int val)
				{
					for (int i = startidx; i < candidates.size(); i++)
					{
						if (candidates[i] == 0) continue;
						tempList.push_back(candidates[i]);
						int tempResult = val - candidates[i];

						if (tempResult > 0)
						{
							backTrack(i, tempResult);
						}
						else if (tempResult == 0)
						{
							Results.push_back(tempList);
						}
						tempList.pop_back();

						if (tempResult < 0)
						{
							// 当期这一层接下来的值没必要考虑了
							break;
						}
					}
				};

			backTrack(0, target);
		}

		return Results;
	}


	/*
	 * 40. https://leetcode.cn/problems/combination-sum-ii/description/
	 * 相比上一次，不用在下一层考虑自己,但是有个问题，candiates是可以重复的.
	 * 所以我们直接先排序，同一层的比较里面碰到相同值直接略过
	 */
	std::vector<std::vector<int>> combinationSum2(std::vector<int>& candidates, int target)
	{
		std::vector<std::vector<int>> Results;

		std::sort(candidates.begin(), candidates.end());

		std::vector<int> tempList;
		// 去重，每一层顶多再需要把自己放进来
		std::function<void(int, int)> backTrack = [&](int startidx, int val)
			{
				for (int i = startidx; i < candidates.size(); i++)
				{
					if (i > startidx && candidates[i] == candidates[i - 1])
					{
						//同一层的相同情况已经考虑过，就放弃，注意这里是 i > startidx，避免candidates[i] 比较到上一层
						continue;
					}

					if (candidates[i] == 0) continue;
					tempList.push_back(candidates[i]);
					int tempResult = val - candidates[i];

					if (tempResult > 0)
					{
						backTrack(i + 1, tempResult);
					}
					else if (tempResult == 0)
					{
						Results.push_back(tempList);
					}
					tempList.pop_back();

					if (tempResult < 0)
					{
						// 当期这一层接下来的值没必要考虑了
						break;
					}
				}
			};

		backTrack(0, target);
		return Results;
	}

	/*
	 * 41. https://leetcode.cn/problems/first-missing-positive/description/
	 * 1. 答案一定在[1, n+1]之间
	 * 2. 限制o(n) 并且常数空间，那就把数组本身当做set用 :让"值 v"待在"下标 v-1"上：
	 *	想想看 数组中的负数和0我们无所谓的，让1去0,2 去 1,3去2.... 自然数 1 ~ n 可以放在长度为n的数组的0 ~ n-1 位置上。
	 *	我们要求v归位到v-1，实际操作中盯着一个归位的位置，如果这个位置上的v 在[1, n]的范围内，那它一定可以归位！
	 *	那么扫描:  下标0是1✓  下标1不是2 ✗ → 答案 = 2
	 */
	int firstMissingPositive(std::vector<int>& nums)
	{
		int n = nums.size();
		for (int i = 0; i < n; i++)
		{
			//其实这里有个关键是认识:while 循环的执行次数,不属于它所在的那次外层迭代,而是被所有外层迭代共享的一个总预算。
			int value = nums[i];
			int valueTargetIdx = nums[i] - 1;
			/*
			 * 1。注意这里用wihle进行保证：我们要求v归位到v-1，实际操作中盯着一个归位的位置，如果这个位置上的v 在[1, n]的范围内，那它一定可以归位！
			 */
			while (value >= 1 && value <= n && nums[valueTargetIdx] != value)
			{
				std::swap(nums[i], nums[valueTargetIdx]);
				value = nums[i];
				valueTargetIdx = nums[i] - 1;
			}
		}

		for (int i = 0; i < n; i++)
		{
			if (nums[i] > 0 && nums[i] != i + 1)
			{
				return i + 1;
			}
		}
		return n + 1;// 1 ~ n 全部就位

	}


	/*
	 * 42. https://leetcode.cn/problems/trapping-rain-water/
	 */
	int trap(std::vector<int>& height)
	{
		{
			//每个位置能接的水 = min(左侧最高, 右侧最高) - 当前高度。用左右两个指针从两端向中间收拢，维护 leftMax 和 rightMax：同时注意掉数组元素非负
			/*
			 * 为什么可以直接先不考虑中间，直接寻找两侧的低点呢？因为只要比边界高的实际上都一定可以先放进来没问题。
			 */
			int left = 0;
			int right = height.size() - 1;
			int leftMax = 0;
			int rightMax = 0;
			int ans = 0;
			while (left < right)
			{
				leftMax = std::max(height[left], leftMax);
				rightMax = std::max(height[right], rightMax);
				if (leftMax < rightMax)
				{
					
					ans += (leftMax - height[left]);
					left++;
				}
				else
				{
					ans += (rightMax - height[right]);
					right--;
				}
			}
			return ans;

		}
		{
			if (height.size() <= 2) return 0;
			int n = height.size();
			int left = 0;
			int right = n - 1;

			// 感觉似乎是从左右两边分别遍历
			int temp = 0;
			int leftPointer = left;
			int rightPointer = right;

			while (left < right)
			{
				int curTemp = 0;
				int compareLeft = height[left];
				for (; leftPointer <= right; leftPointer++)
				{
					if (height[leftPointer] < compareLeft)
					{
						curTemp += compareLeft - height[leftPointer];
					}
					else
					{
						temp += curTemp;
						left = leftPointer;
						curTemp = 0;
					}

					if (height[leftPointer] > compareLeft)
					{
						break;
					}


				}

				// leftPointer 位于第一个比left大的点位上（此时left == leftpointer）/leftPointer = right + 1，left为0 或者 left = right
				/*
				 *  实际统计到的位置在left。
				 */
				curTemp = 0;
				int compareRight = height[right];
				for (; rightPointer >= left; rightPointer--)
				{
					if (height[rightPointer] < compareRight)
					{
						curTemp += compareRight - height[rightPointer];
					}
					else
					{
						temp += curTemp;
						right = rightPointer;
						curTemp = 0;
					}
					if (height[rightPointer] > compareRight)
					{
						break;
					}
				}// rightpointer 位于第一个比right大的点位上（此时right = rightpointer）/ rightpointer = left -1， right为n-1 或者 right = left
			}
			return temp;
		}
	}

	/*
	 * 43. https://leetcode.cn/problems/multiply-strings/description/
	 * 不难，就是细节多 tencent wgx
	 * 下标映射
	 * 进位叠加顺序
	 * chat int的转换方向
	 * 判别"0"的情况
	 */
	std::string multiply(std::string num1, std::string num2)
	{
		std::string& lstr = num1.size() >= num2.size() ? num2 : num1;
		std::string& hstr = num1.size() >= num2.size() ? num1 : num2;

		std::vector<std::deque<int>> temps(lstr.size(), std::deque<int>{});

		std::string reuslt;

		size_t maxColSize = 0;
		// 先得到每一行，然后处理进位
		for (int i = lstr.size() - 1; i >= 0; i--)
		{
			int lineindex = (lstr.size() - 1) - i;
			std::deque<int>& line = temps[lineindex];
			int curvalue = 0;
			int nextvalue = 0;

			for (int j = 0; j <lineindex; j++)
			{
				line.push_back(0);// 直接补0 便于后面计算
			}

			for (int j = hstr.size() - 1; j >= 0; j--)
			{
				int v1 = hstr[j] - '0';
				int v2 = lstr[i] - '0';
				int tempvalue = v1 * v2 + nextvalue;
				curvalue = tempvalue % 10;
				nextvalue = tempvalue / 10;
				line.push_back(curvalue);
			}
			if (nextvalue > 0)
			{
				line.push_back(nextvalue);
			}
			maxColSize = std::max(maxColSize, line.size()); // 最多 ls.size  + 1
		}

		auto getvalue = [&](size_t idx)->int
		{
			int value = 0;
			for (auto& line:temps)
			{
				if (idx < line.size())
				{
					value += line[idx];
				}
			}
			return value;
		};

		int curvalue = 0;
		int nextvalue = 0;
		std::vector<int> results;
		
		for (int i = 0; i < maxColSize; i++)
		{
			int tempvalue =  getvalue(i) + nextvalue;
			curvalue = tempvalue % 10 ;
			nextvalue = tempvalue / 10;
			results.push_back(curvalue);
		}

		
		while (nextvalue > 0)
		{
			int tempvalue = nextvalue;
			curvalue = tempvalue % 10;
			nextvalue = tempvalue / 10;
			results.push_back(curvalue);
		}

		std::string resultstr;
		for (int i = results.size() - 1; i >=0 ;i--)
		{
		
			if (resultstr.empty() && results[i] == 0 && i > 0)
			{
				continue;
			}
			char v = results[i] + '0';
			resultstr.push_back(v);
		}
		return resultstr;
	}

	/*
	 * 44. https://leetcode.cn/problems/wildcard-matching/description/
	 * todo 能想出来这个只能是大量刷题的了
	 * dp[i][j] = s 前 i 个字符能否被 p 前 j 个字符匹配。
	 * p[j] == '*'  >> dp[i][j] = dp[i][j-1] ||  dp[i-1][j] 当前这个字符是*，s的前i个字符能被p的前j-1个字符匹配，那么加上*就肯定能匹配 
	 *	|| s的前i-1个字符能被p的前j个字符匹配。* 已经把 s[1..i-1] 匹配完了。现在多了一个 s[i]，* 可以直接再多吃一个，所以 dp[i][j] 也成立
	 * p[j] == '?'/字符  dp[i][j] = dp[i-1][j-1] 当前这个字符是?/c 要求 s 前 i-1 个字符能被 p 前 j-1 个字符匹配。然后判断当前字符匹配规则。
	 */

	bool isMatch(std::string s, std::string p) 
	{
		{
			int m = s.size();
			int n = p.size();
			std::vector<std::vector<bool>> dp(m + 1, std::vector<bool>(n + 1, false));
			dp[0][0] = true;

			//s 为空时，p 的前 j 个字符能否匹配空串。只有 p 前缀全是* 才行（一旦遇到非* 就断了）。
			for (int j = 1; j <=n; j++)
			{
				dp[0][j] = (p[j - 1] == '*' && dp[0][j - 1]);
			}

			for (int i = 1; i <= m; i++) {
				for (int j = 1; j <= n; j++) {
					if (p[j - 1] == '*')
					{
						dp[i][j] = dp[i][j - 1] || dp[i - 1][j];
					}
					else if (p[j - 1] == '?' || p[j - 1] == s[i - 1])
					{
						dp[i][j] = dp[i - 1][j - 1];
					}
					else
					{
						dp[i][j] = false;
					}
						
				
				}
			}
		}
	}

	/*
	 * 45. https://leetcode.cn/problems/jump-game-ii/
	 * int step <= i + nums[i]
	 * 注意到 如果 i 能跳跃到j，那么i ~ j 的任意位置都能跳到，同时nums[i]越大，那么i就可以越小
	 * 反向贪心符合直接，但是也可以走正向贪心，速度更快 正向贪心的「找最远」容易被误解成「每次都跳到能跳最远的那个点」——其实不是。关键在于它区分了两个东西：
	 * 核心：不是"跳到最远点"，而是"探明这一跳的势力范围"
	 * 想象你手里有「一次跳跃的额度」，这次跳跃能落在一个区间里的任意位置。你不着急决定落在哪，而是先把这个区间里每个点再跳一步能到多远都看一遍，记下最大值 farthest。
	 */
	int jump(std::vector<int>& nums) 
	{
		// pointer 之前可以以最大步长跳到pointer的index
		auto getindex = [&](int pointer)->int
		{
			int leftIndex = 0;
			for (int i = pointer - 1; i >=0; i-- )
			{
				int j = nums[i] + i;
				if (j >= pointer)
				{
					leftIndex = i;
				}
			}
			return leftIndex;
		};

		int firstPointer = nums.size() - 1;
		int index = 0;
		while (firstPointer > 0)
		{
			index++;
			firstPointer = getindex(firstPointer);
		}

		return index;

		{
			int jumps = 0;        // 已跳次数
			int currentEnd = 0;   // 当前这一跳能覆盖到的最远边界
			int farthest = 0;     // 遍历过程中能到达的最远下标

			for (int i = 0; i < (int)nums.size() - 1; i++) {
				farthest = std::max(farthest, i + nums[i]);
				if (i == currentEnd) {     // 走到当前跳的边界，必须再跳一次
					jumps++;
					currentEnd = farthest;
				}
			}
			return jumps;
		}
	}


	/*
	 * 46. https://leetcode.cn/problems/permutations/description/
	 * 其实是深搜，但注意回溯 + 撤销选择, 注意分辨是不是只撤销本轮的选择，下一轮的循环还正常续上
	 */

	std::vector<std::vector<int>> permute(std::vector<int>& nums) 
	{
		std::vector<std::vector<int>> result;
		std::vector<int> tempresults;
		std::function<void(std::set<int>&)> getvalue = [&](std::set<int>& indexs)
			{
				if (indexs.size() == nums.size())
				{
					result.push_back(tempresults);
					return;
				}
				for (int i = 0; i < nums.size(); i++)
				{
					if (indexs.count(i) == 0)
					{
						indexs.emplace(i);
						tempresults.push_back(nums[i]);
						getvalue(indexs);
						// 回溯 撤销本轮选择，记住之类只撤销本轮选择，但是下一轮的循环还正常续上
						indexs.erase(i);
						tempresults.pop_back();
					}
				}
			};

		std::set<int> allindexs;
		getvalue(allindexs);
		return result;
	}


	/*
	 *47. https://leetcode.cn/problems/permutations-ii/description/
	 * 这里要求去重，注意到如果nums[i] == nums[i - 1]的话，那么nums[i] 的组合应该与  nums[i - 1] 一致。
	 * 注意，应该是这一层算完了之后才略过相同的,这里的细节很蛋疼，
	 * 
	 * 官方法：要注意到used[i-1]==true 不是"i-1 用过就丢一边了"，而是"i-1 此刻正躺在 path 里"——是某个祖先层选了它，我们正在它下面继续往深处填数。
	 */
	std::vector<std::vector<int>> permuteUnique(std::vector<int>& nums) 
	{
		{
			std::sort(nums.begin(), nums.end()); // 排序，相同项放在一起
			using namespace std;
			vector<vector<int>> res;
			vector<int> path;
			vector<bool> used(nums.size(), false);
			std::function<void()> backtrack = [&]()
			{
				if (path.size() == nums.size())
				{
					res.push_back(path);
					return;
				}
				for (int i = 0; i <nums.size(); i++)
				{
					if (used[i]) continue;
					//同一层里，跳过与前一个相同、且前一个还没被用的数字.注意这里 !used[i - 1] 不是指这一层没用，而是指当前层/上一层上上一层/祖先层没有用它，这种情况下才能用它
					if (i < 0 && nums[i] == nums[i - 1] && !used[i - 1]) continue;

					used[i] = true;
					path.push_back(nums[i]);
					backtrack();
					path.pop_back();
					used[i] = false;
				}

			};

		}


		std::sort(nums.begin(), nums.end());
		std::vector<std::vector<int>> results;
		std::vector<int> temp;
		std::function<void(std::set<int>& indexs)> getvalue = [&](std::set<int>& indexs)
		{
			if (indexs.size() == nums.size())
			{
				results.push_back(temp);
				return;
			}
			for (int i = 0; i < nums.size();)
			{
				/*不能直接这样搞，nums[i - 1] 不一定被加入过index里面
				 *if (i > 0 && (nums[i] == nums[i - 1]))
				{
					continue;
				}*/


				if (indexs.count(i) == 0)
				{
					indexs.emplace(i);
					temp.push_back(nums[i]);
					getvalue(indexs);
					temp.pop_back();
					indexs.erase(i);
					// 应该在保证相同的i已经参与过组合之后，我们才排除相同的i + 1项
					i++;
					while (i < nums.size() && (nums[i] == nums[i - 1]))
					{
						i++;
					}
				}
				else
				{
					i++;
				}
			
			

			}

		};
		std::set<int> indexs;
		getvalue(indexs);
		return results;
	}

	/*
	 * 48. https://leetcode.cn/problems/rotate-image/description/
	 * 这一题理应熟悉矩阵变换的性质
	 */
	void rotate(std::vector<std::vector<int>>& matrix) 
	{
		int n = matrix.size();
		/*
		 * k 行 》 n - 1 - k 列 交换，元素遍历顺序为顺序遍历
		 * [i][j] >> [j][n - 1 -i]
		 * 
		 * 先上下调换，然后x，y镜像
		 */
		 



	}
};

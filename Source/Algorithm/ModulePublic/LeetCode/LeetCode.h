#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
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

	/*
	* 4. https://leetcode.cn/problems/median-of-two-sorted-arrays/description/
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
			int l1 = 0;
			int l2 = 0;
			int midIndex = (nums1.size() + nums2.size()) / 2;
			int midIndex2 = ((nums1.size() + nums2.size()) % 2 == 0) ? midIndex : (midIndex + 1);
			int p = 0;
			while (p < nums1.size() || p < nums2.size())
			{
				while (l1 < nums1.size() && nums1[l1] <= nums2[l2])
				{
					l1++;
					if (l1 == midIndex)
					{
						// 到达中位数节点
						int value1 = nums1[l1];
						int value2 = value1;
						if (l1 == nums1.size() - 1)
						{
							value2 = nums2[l2];
						}
						else
						{
							if (nums1[l1 + 1] <= nums2[l2])
							{
								value2 = nums1[l1 + 1];
							}
							else
							{
								value2 = nums2[l2];
							}
						}
						return (value1 + value2) / 2.0;
					
					}
				}

				while (l2 < nums2.size() && nums2[l2] < nums1[l1])
				{
					l2++;
					if (l2 == midIndex)
					{
						int value1 = nums2[l2];
						int value2 = value1;
						if (l2 == nums2.size() - 1)
						{
							value2 = nums1[l1];
						}
						else
						{
							if (nums2[l2 + 1] < nums1[l1])
							{
								value2 = nums2[l2 + 1];
							}
							else
							{
								value2 = nums1[l1];
							}
						}
						return (value1 + value2) / 2.0;
					}
				}

				p++;
			}
			return 0;
		}
	}
};
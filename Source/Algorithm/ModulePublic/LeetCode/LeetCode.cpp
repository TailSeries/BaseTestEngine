#include "LeetCode.h"
#include <vector>

class SolutionForReview
{
public:
	struct ListNode
	{
		int val;
		ListNode* next;
		ListNode() : val(0), next(nullptr) {}
		ListNode(int x) : val(x), next(nullptr) {}
		ListNode(int x, ListNode* next) : val(x), next(next) {}
	};

	/*2.  https://leetcode.cn/problems/add-two-numbers/
	给你两个 非空 的链表，表示两个非负的整数。它们每位数字都是按照 逆序 的方式存储的，并且每个节点只能存储 一位 数字。
	* 技巧：
	1.链表不等长，其中一个为空的位置认为是0就可以了。
	2.这种题目没限制原地修改，那你就新建，更为清晰简明
	* 
	* 链表要始终注意head所在的位置，每次步进操作的时候注意node = node->next的时候，node所在的真实位置
	* 如果链表不等长，对应位置判空，默认它是0就可以了。
	* 满足 			if (resolvevalue > 0 || l1 || l2) 条件的时候，就应该要新建Head的next了
	*/
	ListNode* addTwoNumbers(ListNode* l1, ListNode* l2)
	{
		ListNode* Head = new ListNode(0);
		ListNode* Result = Head;
		int resolvevalue = 0;
		while (l1 || l2)
		{
			int curvalue = (l1 == nullptr ? 0 : l1->val) + (l2 == nullptr ? 0 : l2->val) + resolvevalue;
			resolvevalue = curvalue / 10;
			curvalue = curvalue % 10;
			Head->val = curvalue;
			l1 = l1 ? l1->next : nullptr;
			l2 = l2 ? l2->next : nullptr;
			if (resolvevalue > 0 || l1 || l2)
			{
				Head->next = new ListNode(resolvevalue);
				Head = Head->next;
			}


		}
		return Result;
	}

	/* 3. https://leetcode.cn/problems/longest-substring-without-repeating-characters/
	给定一个字符串 s ，请你找出其中不含有重复字符的 最长 子串 的长度。
	* 
	*/

	/*
	* 4.  https://leetcode.cn/problems/median-of-two-sorted-arrays/description/
	* 给定两个大小分别为 m 和 n 的正序（从小到大）数组 nums1 和 nums2。请你找出并返回这两个正序数组的 中位数 。
	* 归并能解决问题，但是算法不最优。
	*
	* 中位数特性：不用真正合并数组，只要找到一个划分，让左半部分的所有数<=右半部分。max(aLeft, bLeft) < min(aRight, bRight)
	技巧：	1.保证A是比较短的数据
			if (nums1.size() > nums2.size())
			{
				return findMedianSortedArrays(nums2, nums1);
			}
	*/

	/*
	 * 6. https://leetcode.cn/problems/zigzag-conversion/
	 *  关键：每行都是一个新字符串，关键在于看穿字符串遍历顺序与行字符串之间的关系
	 */



};


void TestSolutionForReview()
{
	SolutionForReview s;
	return;
}

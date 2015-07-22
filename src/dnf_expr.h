// Copyright (c) 2015, lokicui@gmail.com. All rights reserved.
#ifndef SRC_DNF_EXPR_H
#define SRC_DNF_EXPR_H
#pragma once
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <tr1/functional>
#include "src/std_def.h"

template <typename T>
class TypeTraits
{
private:
    template<class U> struct PointerTraits
    {
        enum { result = false };
        typedef U* PointerType;
    };

    template<class U> struct PointerTraits<U*>
    {
        enum { result = true };
        typedef U* PointerType;
    };
public:
    enum { isPointer = PointerTraits<T>::result };
    typedef typename PointerTraits<T>::PointerType PointerType;
};

// 析取范式
// 类型U必须有两个方法
//  1、size_t get_df() 返回每个term的df值
//  2、bool seek_lower_bound(pageid_t *cdocid, pageid_t predocid);
//         return true, if found cdocid greater then predocid
//         return false, if reach the end
template <typename U>
class DNFExpr
{
public:
    typedef typename TypeTraits<U>::PointerType T;

    // candidate definition
    typedef class Candidate
    {
    public:
        // candidate中存QueryTerm *
        // candidate必须对QueryTerm进行排序
        typedef typename std::vector<T>::iterator iterator;
        typedef typename std::vector<T>::const_iterator const_iterator;
    public:
        void sort()
        {
            // @todo
            // 可先按照term的DF进行排序
        }
        T at(size_t i) const { return terms_.at(i); }
        bool empty() const { return terms_.empty(); }
        size_t size() const { return terms_.size(); }
        iterator begin() { return terms_.begin(); }
        iterator end() { return terms_.end(); }
        const_iterator begin() const { return terms_.begin(); }
        const_iterator end() const { return terms_.end(); }
        void push_back(const T& val)
        {
            terms_.push_back(val);
        }
        void assign(iterator first, iterator last)
        {
            terms_.assign(first, last);
        }
    private:
        static bool cmp(const T lhs, const T rhs)
        {
            return lhs->get_df() < rhs->get_df();
        }
        std::vector<T> terms_;
    }candidate_t;

    typedef class Node
    {
    public:
        Node():data_(NULL), parent_(NULL), fail_(NULL), succ_(NULL), candidate_(NULL)
        {}
        Node(T t, Node* parent):data_(t), parent_(parent), fail_(NULL),
                                            succ_(NULL), candidate_(NULL)
        {}
        Node(T t, Node* parent, Node* succ, Node* fail):data_(t), parent_(parent),
                                        fail_(fail), succ_(succ), candidate_(NULL)
        {}
        ~Node()
        {
        }
        T get()
        {
            return data_;
        }
        T data_;
        Node *parent_;
        Node *fail_;
        Node *succ_;
        candidate_t *candidate_; // candidate_不为NULL, 代表这是一个candidate的ending point
    }node_t;

public:
    DNFExpr():root_(new node_t())
    {
        assert(root_);
        root_->parent_ = root_;
        root_->succ_ = root_;
        root_->fail_ = root_;
    }

    ~DNFExpr()
    {
        assert(root_);
        node_t *p(root_->succ_);
        while ( p != root_)
        {
            node_t *tmp(p);
            p = p->succ_;
            delete tmp;
        }
        delete root_;
    }

    // return the first doc which greater than predoc and find all candidates this doc matched.
    bool next(pageid_t *docid, std::vector<const candidate_t*> *match_candidates, pageid_t predoc)
    {
        node_t *p(root_->succ_);
        if (p == root_)
            return false;
        const pageid_t k_max_doc = pageid_t(-1);
        pageid_t mindoc(k_max_doc);
        pageid_t cdoc;
        while ( ++predoc < k_max_doc )
        {
            while (p != root_)
            {
                if (!p->get()->seek_lower_bound(&cdoc, predoc))
                    cdoc = k_max_doc;
                // find one
                if (predoc == cdoc)
                {
                    // hit one candidate, we must check the other candidates.
                    if (p->candidate_)
                    {
                        match_candidates->push_back(p->candidate_);
                        p = p->succ_;
                        *docid = cdoc;
                        goto out;
                    }
                    p = p->succ_;
                }
                else
                {
                    //predoc != cdoc
                    mindoc = std::min(mindoc, cdoc - 1);
                    p = p->fail_;
                }
            }
            // predoc have no matched candidate
            predoc = mindoc;
            mindoc = k_max_doc;
            // NND 调试了半天就因为这个
            p = p->succ_;   // 在这里他的值一定是 root_->succ_;
        }
        if (predoc == k_max_doc)
            return false;
out:
        // already hit one candidate
        while (p != root_)
        {
            if (p->get()->seek_lower_bound(&cdoc, predoc) && predoc == cdoc)
            {
                if (p->candidate_)
                    match_candidates->push_back(p->candidate_);
                p = p->succ_;
            }
            else
            {
                p = p->fail_;
            }
        }
        return true;
    }

    int32_t add(const candidate_t* candidate)
    {
        if (!candidate || candidate->empty())
            return -1;
        node_t *p(root_->succ_);
        size_t i(0);
        size_t k = hash(candidate->at(i));
        do
        {
            if (hash(p->get()) == k)
            {
                p = p->succ_;
                if ( ++i < candidate->size())
                    k = hash(candidate->at(i));
                else
                    break;
            }
            else
            {
                // 本是同根生
                if (p->fail_->parent_ == p->parent_)
                    p = p->fail_;
                else
                    break;
            }
        } while (p != root_);
        p = p->parent_;
        node_t *next = p->succ_;
        for (; i < candidate->size(); ++i)
        {
            node_t *node = new node_t(candidate->at(i), p, next, next);
            p->succ_ = node;
            p = node;
        }
        // 除非加入了重复的candidate
        assert(p->candidate_ == NULL);
        p->candidate_ = const_cast<candidate_t*>(candidate);
        return 0;
    }

    int32_t remove(const candidate_t* candidate)
    {
        // 确认存在这个candidate
        // 所有fail_指向这个node的都要改指向node->fail_
        if (!candidate || candidate->empty())
            return -1;
        node_t *p(root_->succ_);
        size_t i(0);
        size_t k = hash(candidate->at(i));
        do
        {
            if (hash(p->get()) == k)
            {
                p = p->succ_;
                if ( ++i < candidate->size())
                    k = hash(candidate->at(i));
                else
                    break;
            }
            else
            {
                // 本是同根生
                if (p->fail_->parent_ == p->parent_)
                    p = p->fail_;
                else
                    break;
            }
        } while (p != root_);

        // find
        p = p->parent_;
        if ( p->pattern_ != candidate || i != candidate->size())
            return -2;
        //
        // 说明:
        //      -f->  代表fail_指针
        //      -sf-> 代表succ_和fail_指针
        //      没有标注的都是succ_指针
        //      parent指针没有画出来
        //      左侧B->parent_ = root_
        //      左侧C->parent_ = 左侧B
        //      左侧D->parent_ = 左侧C
        //      右侧A->parent_ = root_
        //      右侧B->parent_ = 右侧A
        //      右侧C->parent_ = 右侧B
        //      右侧D->parent_ = 右侧C
        //      右侧D,E,F的parent_ 都指向C
        //
        //              R
        //             /
        //            B -f-> A
        //           /      / \
        //          C---f->/   B
        //         /      /     \
        //        D---sf->       C
        //                      /
        //                     D-sf->E-sf->F-sf->Root
        //
        // 叶子节点
        node_t *leaf(p);
        p = p->parent_;
        // 假定现在需要删除BCD这个candidate
        // 在这里依次删除了 D C, 到B的时候就不满足条件了,
        // 证明B还有兄弟节点(A->parent_ == B->parent_ == root_)
        while ( p->succ_ == leaf && p->fail_ == leaf->fail_)
        {
            p->succ_ = leaf->succ_;
            // p->fail_ = leaf->fail_;
            delete leaf;
            leaf = p;
            p = p->parent_;
        }
        // p已经指向R了, leaf指向B, 而且B有兄弟节点,
        // 那把B->parent_所有的儿子整理一下就ok了(链表删除B节点)
        // BFS遍历一下
        do
        {
            // 为什么p->fail_ == leaf的时候不能break呢, 考虑删除ABCD这个candidate
            if (p->fail_ == leaf)
            {
                p->fail_ = leaf->fail_;
            }
            if (p->succ_ == leaf)
            {
                p->succ_ = leaf->succ_;
                // p->fail_ = leaf->fail_;  // 当删除D的时候会有问题, 因为C的fail_指针可不是指向D的,所以不能这么搞
                delete leaf;
                break;
            }
            // 遍历兄弟节点
            p = p->succ_; // p = p->fail_也可以
        } while (p != root_);
        return 0;
    }

private:
    size_t hash(const T v) const
    {
        return hash_(v);
    }

private:
    node_t *root_;
    std::tr1::hash<T> hash_;
};
#endif // SRC_DNF_EXPR_H

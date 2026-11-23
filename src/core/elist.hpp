// 优先级单向链表：位置即优先级
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "model.hpp"

namespace moji {

class EventList {
public:
    struct Node {
        Event event;
        Node* next = nullptr;
    };

    EventList() = default;
    ~EventList();
    EventList(const EventList&) = delete;
    EventList& operator=(const EventList&) = delete;

    class Iterator {
    public:
        explicit Iterator(Node* node) : node_(node) {}
        Event& operator*() const { return node_->event; }
        Event* operator->() const { return &node_->event; }
        Iterator& operator++() {
            node_ = node_->next;
            return *this;
        }
        bool operator!=(const Iterator& other) const { return node_ != other.node_; }

    private:
        Node* node_;
    };

    Iterator begin() const { return Iterator(head_); }
    Iterator end() const { return Iterator(nullptr); }

    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    Event* find(const std::string& id);
    const Event* find(const std::string& id) const;
    int index_of(const std::string& id) const;  // 1 起始，找不到 -1
    Event* at(int pos);                          // 1 起始

    Event& append(Event event);
    Event& insert_at(int pos, Event event);
    std::optional<Event> remove(const std::string& id);
    bool move_to(const std::string& id, int pos);
    bool move_by(const std::string& id, int delta);

    void reindex();
    void clear();
    void load(std::vector<Event> events);

    std::vector<Event*> views();
    std::vector<Event> all_copy() const;
    std::vector<json> to_json_array() const;

private:
    Node* head_ = nullptr;
    Node* tail_ = nullptr;
    size_t size_ = 0;
};

}  // namespace moji

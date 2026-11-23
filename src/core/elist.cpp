#include "elist.hpp"

#include <algorithm>

namespace moji {

EventList::~EventList() { clear(); }

void EventList::clear() {
    Node* node = head_;
    while (node) {
        Node* next = node->next;
        delete node;
        node = next;
    }
    head_ = tail_ = nullptr;
    size_ = 0;
}

Event* EventList::find(const std::string& id) {
    for (Node* n = head_; n; n = n->next) {
        if (n->event.id == id) {
            return &n->event;
        }
    }
    return nullptr;
}

const Event* EventList::find(const std::string& id) const {
    for (Node* n = head_; n; n = n->next) {
        if (n->event.id == id) {
            return &n->event;
        }
    }
    return nullptr;
}

int EventList::index_of(const std::string& id) const {
    int pos = 1;
    for (Node* n = head_; n; n = n->next, ++pos) {
        if (n->event.id == id) {
            return pos;
        }
    }
    return -1;
}

Event* EventList::at(int pos) {
    if (pos < 1) {
        return nullptr;
    }
    Node* n = head_;
    for (int i = 1; n && i < pos; ++i) {
        n = n->next;
    }
    return n ? &n->event : nullptr;
}

Event& EventList::append(Event event) {
    Node* node = new Node{std::move(event), nullptr};
    if (!head_) {
        head_ = tail_ = node;
    } else {
        tail_->next = node;
        tail_ = node;
    }
    ++size_;
    tail_->event.priority = (int)size_;
    return tail_->event;
}

Event& EventList::insert_at(int pos, Event event) {
    int total = (int)size_;
    if (pos < 1) {
        pos = 1;
    }
    if (pos > total + 1) {
        pos = total + 1;
    }
    // 找到前驱（pos == 1 时前驱为空）
    Node* prev = nullptr;
    Node* cur = head_;
    for (int i = 1; i < pos && cur; ++i) {
        prev = cur;
        cur = cur->next;
    }
    Node* node = new Node{std::move(event), cur};
    if (prev) {
        prev->next = node;
    } else {
        head_ = node;
    }
    if (!node->next) {
        tail_ = node;
    }
    ++size_;
    reindex();
    return node->event;
}

std::optional<Event> EventList::remove(const std::string& id) {
    Node* prev = nullptr;
    Node* cur = head_;
    while (cur) {
        if (cur->event.id == id) {
            if (prev) {
                prev->next = cur->next;
            } else {
                head_ = cur->next;
            }
            if (tail_ == cur) {
                tail_ = prev;
            }
            Event out = cur->event;
            delete cur;
            --size_;
            reindex();
            return out;
        }
        prev = cur;
        cur = cur->next;
    }
    return std::nullopt;
}

bool EventList::move_to(const std::string& id, int pos) {
    Event* found = find(id);
    if (!found) {
        return false;
    }
    Event copy = *found;
    remove(id);
    insert_at(pos, copy);
    return true;
}

bool EventList::move_by(const std::string& id, int delta) {
    int pos = index_of(id);
    if (pos < 0) {
        return false;
    }
    int target = pos + delta;
    target = std::max(1, std::min((int)size_, target));
    return move_to(id, target);
}

void EventList::reindex() {
    int p = 1;
    for (Node* n = head_; n; n = n->next) {
        n->event.priority = p++;
    }
}

void EventList::load(std::vector<Event> events) {
    clear();
    for (auto& e : events) {
        append(std::move(e));
    }
    reindex();
}

std::vector<Event*> EventList::views() {
    std::vector<Event*> out;
    out.reserve(size_);
    for (Node* n = head_; n; n = n->next) {
        out.push_back(&n->event);
    }
    return out;
}

std::vector<Event> EventList::all_copy() const {
    std::vector<Event> out;
    out.reserve(size_);
    for (Node* n = head_; n; n = n->next) {
        out.push_back(n->event);
    }
    return out;
}

std::vector<json> EventList::to_json_array() const {
    std::vector<json> out;
    out.reserve(size_);
    for (Node* n = head_; n; n = n->next) {
        out.push_back(n->event.to_json());
    }
    return out;
}

}  // namespace moji

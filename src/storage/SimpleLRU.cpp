#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value)
{
    if(key.size()+value.size() > _max_size)
    {
        return false;
    }

    while(key.size()+value.size() > _free_space)
    {
        SimpleLRU::_Delete_tail_node();
    }

    lru_node* node = new lru_node;
    node->key = key;
    node->value = value;

    _free_space = _free_space - key.size() - value.size();

    SimpleLRU::_Insert_in_list(node);
    SimpleLRU::_Insert_in_storage(node);

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value)
{
    auto item = _lru_index.find(key);
    if (item != _lru_index.end()) {
        return false;
    }

    return SimpleLRU::Put(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value)
{
    auto item = _lru_index.find(key);
    if (item != _lru_index.end()) {
        return false;
    }

    return SimpleLRU::Put(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key)
{
    auto item = _lru_index.find(key);
    if (item != _lru_index.end()) {
        return false;
    }

    lru_node* buff = &_lru_index.at(key).get();

    if(buff == _lru_tail.get())
    {
        return SimpleLRU::_Delete_tail_node();
    }

    size_t del_space = key.size() + buff->value.size();

    if(buff == _lru_head)
    {
        _lru_head = buff->prev;

        if(_lru_head != nullptr)
        {
            _lru_head->next.reset(nullptr);
        }
    }
    else
    {
        buff->next.get()->prev = buff->prev;
        buff->prev->next.reset(buff->next.get());
    }

    _lru_index.erase(key);
    _free_space = _free_space + del_space;

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) const
{
    auto item = _lru_index.find(key);
    if (item == _lru_index.end()) {
        return false;
    }

    lru_node* buff = &_lru_index.at(key).get();

    value = buff->value;
    if(buff == _lru_head)
    {
        return true;
    }

    if(buff->prev == nullptr)
    {
        _lru_tail.swap(buff->next);
        _lru_tail.get()->prev = nullptr;
        buff->prev = _lru_head;
        _lru_head->next.swap(buff->next);
        return true;
    }

    buff->next.get()->prev = buff->prev;
    buff->prev->next.swap(buff->next);
    buff->prev = _lru_head;
    _lru_head->next.swap(buff->next);

    return true;
}

//Inner methods
bool SimpleLRU::_Delete_tail_node()
{
    lru_node* buff = _lru_tail.get();
    if(buff == nullptr)
    {
        return false;
    }

    _lru_index.erase(buff->key);
    size_t dm = buff->key.size() + buff->value.size();

    _lru_tail.swap(buff->next);
    _lru_tail.get()->prev = nullptr;
    buff->next.reset(nullptr);
    _free_space += dm;
    return true;
}

void SimpleLRU::_Insert_in_list(lru_node* const _node)
{
    if(_lru_head == nullptr)
    {
        _lru_head = _node;
        _lru_tail.reset(_node);
        return;
    }

    _lru_head->next.reset(_node);
    _node->prev = _lru_head;
    _lru_head = _node;
}

void SimpleLRU::_Insert_in_storage(lru_node* const _node)
{
    std::reference_wrapper<lru_node> ref_node(*_node);

    if(_lru_index.count(_node->key)==0)
    {
        std::reference_wrapper<const std::string> ref_key(_node->key);
        _lru_index.insert(std::pair<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>>(ref_key, ref_node));
    }
    else
    {
        _lru_index.at(_node->key) = ref_node;
    }
}

} // namespace Backend
} // namespace Afina


















/*#include "SimpleLRU.h"
#include <iostream>

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value)
{
	auto it = _lru_index.find(std::reference_wrapper<const std::string>(key));
	if (it != _lru_index.end())
  {
		return _Update_in_list(it->second.get(), value);
	}
	return _Insert_to_list(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value)
{
	if (_lru_index.find(std::reference_wrapper<const std::string>(key)) != _lru_index.end())
  {
		return false;
  }
	return _Insert_to_list(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value)
{
	auto it = _lru_index.find(std::reference_wrapper<const std::string>(key));
	if (it == _lru_index.end())
  {
		return false;
  }
	return _Update_in_list(it->second.get(), value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
	auto it = _lru_index.find(std::reference_wrapper<const std::string>(key));
	if (it == _lru_index.end())
  {
		return false;
  }
	lru_node & node = it->second.get();
	_lru_index.erase(it);
	return _Erase_from_list(node);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value)
{
	auto it = _lru_index.find(std::reference_wrapper<const std::string>(key));
	if (it == _lru_index.end())
  {
		return false;
  }
	value = it->second.get().value;
	return _Update_in_list(it->second.get(), value);
}

bool SimpleLRU::_DeleteOld()
{
	if (_lru_head == nullptr)
  {
		return false;
  }
	lru_node & node = *_lru_head;
	_lru_index.erase(_lru_head->key);
	return _Erase_from_list(node);
}

bool SimpleLRU::_Insert_to_list(const std::string &key, const std::string &value)
{
	std::unique_ptr<lru_node> ptr(new lru_node(key, value, _lru_tail));
	lru_node * tmp = ptr.get();
	ptr.swap(ptr->next);
	if (!_insert_ptr(*tmp))
  {
		return false;
  }
	_lru_index.insert(std::pair<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>>(_lru_tail.get()->key, _lru_tail.get()));
	return true;
}

bool SimpleLRU::_Erase_from_list(lru_node &node)
{
	if (!_erase_ptr(node))
  {
		return false;
  }
	node.next.reset(nullptr);
	return true;
}

bool SimpleLRU::_Update_in_list(lru_node &node, const std::string &value)
{
	if (!_erase_ptr(node))
  {
		return false;
  }
	if (!_insert_ptr(node))
  {
		return false;
  }
	_lru_tail.get()->value = value;
	return true;
}

bool SimpleLRU::_insert_ptr(lru_node &node)
{
std::size_t len = node.key.size() + node.value.size();
	if (len > _max_size)
  {
		return false;
  }
	while (len + _cur_size > _max_size)
  {
		if (!_DeleteOld())
    {
			return false;
    }
	}

	_cur_size += len;

	if (_lru_tail.get() != nullptr)
  {
		_lru_tail.get()->next.swap(node.next);
		node.prev.reset(_lru_tail); //??
		_lru_tail.reset(_lru_tail.get()->next.get());
	}
	else
  {
		_lru_head.swap(node.next);
		node.prev.reset(_lru_tail.get());
		_lru_tail.reset(_lru_head.get());
	}
	return true;
}

bool SimpleLRU::_erase_ptr(lru_node &node)
{
	std::size_t len = node.key.size() + node.value.size();
	_cur_size -= len;
	if (node.next != nullptr)
  {
		node.next->prev = node.prev;
  }
	else
  {
		_lru_tail.reset(node.prev);
  }

	if (node.prev != nullptr)
  {
		node.prev->next.swap(node.next);
  }
	else
  {
		_lru_head.swap(node.next);
  }
	return true;
}

} // namespace Backend
} // namespace Afina

*/

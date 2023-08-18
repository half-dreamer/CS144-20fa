#include "stream_reassembler.hh"
#include <cassert>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _unassemble_strs()
    , _next_assembled_idx(0)
    , _unassembled_bytes_num(0)
    , _eof_idx(-1)
    , _output(capacity)
    , _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
 
    // 获取 map 中第一个大于 index 的迭代器指针
    auto pos_iter = _unassemble_strs.upper_bound(index);
    // 尝试获取一个小于等于 index 的迭代器指针
    if (pos_iter != _unassemble_strs.begin())
        pos_iter--;
    /**
     *  此时迭代器有三种情况，
     *  1. 一种是为 end_iter，表示不存在任何数据
     *  2. 一种是为非 end_iter，
     *      a. 一种表示确实存在一个小于等于 idx 的迭代器指针
     *      b. 一种表示当前指向的是大于 idx 的迭代器指针
     */

    // 获取当前子串的新起始位置
    size_t new_idx = index;
    // 如果前面确实有子串
    if (pos_iter != _unassemble_strs.end() && pos_iter->first <= index) {
        const size_t up_idx = pos_iter->first;

        // 如果当前子串前面出现了重叠
        if (index < up_idx + pos_iter->second.size())
            new_idx = up_idx + pos_iter->second.size();
    }
    // 如果前面没有子串，则和当前读取到的pos进行比较
    else if (index < _next_assembled_idx)
        new_idx = _next_assembled_idx;

    // 子串新起始位置对应到的 data 索引
    const size_t data_start_pos = new_idx - index;
    // 当前子串将保存的 data 的长度
    /**
     *  NOTE: 注意，若上一个子串将当前子串完全包含，则 data_size 不为正数
     *  NOTE: 这里由于 unsigned 的结果向 signed 的变量写入，因此可能存在上溢的可能
     *        但即便上溢，最多只会造成当前传入子串丢弃，不会产生任何安全隐患
     *  PS: 而且哪个数据包会这么大，大到超过 signed long 的范围
     */
    ssize_t data_size = data.size() - data_start_pos;

    // 获取当前子串的下个子串时，需要考虑到 new_idx 可能会和 down_idx 重合
    pos_iter = _unassemble_strs.lower_bound(new_idx);
    // 下面注释的这两条语句与上面的 lower_bound 等价
    // if (pos_iter != _unassemble_strs.end() && pos_iter->first <= new_idx)
    //     ++pos_iter;

    // 如果和下面的子串冲突，则截断长度
    while (pos_iter != _unassemble_strs.end() && new_idx <= pos_iter->first) {
        const size_t data_end_pos = new_idx + data_size;
        // 如果存在重叠区域
        if (pos_iter->first < data_end_pos) {
            // 如果是部分重叠
            if (data_end_pos < pos_iter->first + pos_iter->second.size()) {
                data_size = pos_iter->first - new_idx;
                break;
            }
            // 如果是全部重叠
            else {
                _unassembled_bytes_num -= pos_iter->second.size();
                pos_iter = _unassemble_strs.erase(pos_iter);
                continue;
            }
        }
        // 如果不重叠则直接 break
        else
            break;
    }
    // 检测是否存在数据超出了容量。注意这里的容量并不是指可保存的字节数量，而是指可保存的窗口大小
    //! NOTE: 注意这里我们仍然接收了 index 小于 first_unacceptable_idx  但
    //        index + data.size >= first_unacceptable_idx 的那部分数据
    //        这是因为处于安全考虑，最好减少算术运算操作以避免上下溢出
    //        同时多余的那部分数据最多也只会多占用 1kb 左右，属于可承受范围之内
    size_t first_unacceptable_idx = _next_assembled_idx + _capacity - _output.buffer_size();
    if (first_unacceptable_idx <= new_idx)
        return;

    // 判断是否还有数据是独立的， 顺便检测当前子串是否被上一个子串完全包含
    if (data_size > 0) {
        const string new_data = data.substr(data_start_pos, data_size);
        // 如果新字串可以直接写入
        if (new_idx == _next_assembled_idx) {
            const size_t write_byte = _output.write(new_data);
            _next_assembled_idx += write_byte;
            // 如果没写全，则将其保存起来
            if (write_byte < new_data.size()) {
                // _output 写不下了，插入进 _unassemble_strs 中
                const string data_to_store = new_data.substr(write_byte, new_data.size() - write_byte);
                _unassembled_bytes_num += data_to_store.size();
                _unassemble_strs.insert(make_pair(_next_assembled_idx, std::move(data_to_store)));
            }
        } else {
            const string data_to_store = new_data.substr(0, new_data.size());
            _unassembled_bytes_num += data_to_store.size();
            _unassemble_strs.insert(make_pair(new_idx, std::move(data_to_store)));
        }
    }

    // 一定要处理重叠字串的情况
    for (auto iter = _unassemble_strs.begin(); iter != _unassemble_strs.end(); /* nop */) {
        // 如果当前刚好是一个可被接收的信息
        assert(_next_assembled_idx <= iter->first);
        if (iter->first == _next_assembled_idx) {
            const size_t write_num = _output.write(iter->second);
            _next_assembled_idx += write_num;
            // 如果没写全，则说明写满了，保留剩余没写全的部分并退出
            if (write_num < iter->second.size()) {
                _unassembled_bytes_num += iter->second.size() - write_num;
                _unassemble_strs.insert(make_pair(_next_assembled_idx, std::move(iter->second.substr(write_num))));

                _unassembled_bytes_num -= iter->second.size();
                _unassemble_strs.erase(iter);
                break;
            }
            // 如果写全了，则删除原有迭代器，并进行更新
            _unassembled_bytes_num -= iter->second.size();
            iter = _unassemble_strs.erase(iter);
        }
        // 否则直接离开
        else
            break;
    }
    if (eof)
        _eof_idx = index + data.size();
    if (_eof_idx <= _next_assembled_idx)
        _output.end_input();   




















    /** shit moutain. one test can't pass.
        // index out of range
        if (index > _output.bytes_read() + _capacity - 1) {
            return;
        }
        string new_data = data;
        if (index + new_data.length() - 1 > _output.bytes_read() + _capacity) {
            if (new_data != "") {
                new_data.resize(_output.bytes_read() + _capacity - index);
            }
        }

        // index inside range
        // 1. eof condition
        if (eof) {
            eofSign = true;
            eofByte = index + new_data.length() - 1;
        }
        //  index <= nextByte condition
        if (index <= nextByte) {
            if (index + new_data.length() < nextByte) {
                return;
            } else {
                // I think it's a better way to write in the last session of the function
                window[nextByte] = new_data.substr(nextByte - index, index + new_data.length() - nextByte);
                unAssembledBytes += index + new_data.length() - nextByte;

                // size_t beReadBytes = _output.write(data.substr(nextByte - index, index + data.length() - nextByte));
                // nextByte += beReadBytes;
                // unAssembledBytes -=
            }
        }
        // index > nextByte
        else {
            auto it_head = window.upper_bound(index);
            if (it_head != window.begin()) {
                it_head--;
                if (it_head->first + window[it_head->first].length() - 1 < index) {
                    window[index] = new_data;
                    unAssembledBytes += new_data.length();
                } else {
                    if (it_head->first + window[it_head->first].length() - 1 <= index + new_data.length() - 1) {
                        // unAssembledBytes -= window[it_head->first].length();
                        // window[index] = new_data;
                        // window[it_head->first].resize(index - it_head->first);
                        // unAssembledBytes += window[index].length() + window[it_head->first].length();

                        unAssembledBytes -= window[it_head->first].length();
                        window[it_head->first] =
                            window[it_head->first] +
                            new_data.substr(it_head->first + window[it_head->first].length() - 1 - index + 1, index +
       new_data.length() - 1 - (it_head->first + window[it_head->first].length() - 1)); unAssembledBytes +=
       window[it_head->first].length(); } else { return;
                    }
                }
            } else {
                if (window.count(index)) {
                    unAssembledBytes -= window[index].length();
                }
                window[index] = new_data;
                unAssembledBytes += new_data.length();
                ;
            }

            auto it_tail = window.upper_bound(index + new_data.length() - 1);
            auto it = it_head;
            size_t middleIndex = -1;
            while (it != it_tail) {
                if (it->first > index && it->first <= index + new_data.length() - 1) {
                    middleIndex = it->first;
                    if (middleIndex + window[middleIndex].length() -1 < index + window[index].length() - 1) {
                        unAssembledBytes -= window[middleIndex].length();
                        it = window.erase(it);
                        continue;
                    } else {
                        if (!window[index].empty()) {
                            window[index] = new_data + window[middleIndex].substr(index + window[index].length() - 1 -
       middleIndex, middleIndex + window[middleIndex].length() - 1 - (index + window[index].length() - 1) + 1);
                            unAssembledBytes -= (index + new_data.length() - 1) - middleIndex + 1;
                            it = window.erase(it);
                            continue;
                        } else {
                            auto it_prev = prev(prev(it));
                            window.erase(prev(it));
                            window[it_prev->first] += window[middleIndex].substr(
                                it_prev->first + window[it_prev->first].length() - 1 - middleIndex,
                                middleIndex + window[middleIndex].length() - 1 -
                                    (it_prev->first + window[it_prev->first].length() - 1) + 1);
                            unAssembledBytes -= (it_prev->first + window[it_prev->first].length() - 1) - middleIndex +
       1; it = window.erase(it); continue;
                        }
                    }
                }
                it++;
            }
        }
        if (eofSign && new_data == "") {
            _output.end_input();
        }
        vector<size_t> eraseKeys{};
        for (auto it_window : window) {
            size_t writtenByte = 0;
            if (it_window.first <= nextByte) {
                eraseKeys.push_back(it_window.first);

                if (it_window.first + it_window.second.length() - 1 < nextByte) {
                    unAssembledBytes -= it_window.second.length();
                    continue;
                }
                if (it_window.first + it_window.second.length() - 1 > eofByte && eofSign) {
                    writtenByte = _output.write(it_window.second.substr(nextByte - it_window.first, eofByte - nextByte +
       1)); unAssembledBytes -= writtenByte; nextByte += writtenByte; _output.end_input(); break;
                }
                writtenByte = _output.write(
                    it_window.second.substr(nextByte - it_window.first, it_window.first + it_window.second.length() - 1
       - nextByte + 1)); if (writtenByte != window[it_window.first].length() && writtenByte > 0) { if (nextByte +
       writtenByte - 1 == it_window.first + it_window.second.length() - 1) {
                        ;
                    } else {
                    window[nextByte + writtenByte] =
                        window[it_window.first].substr(nextByte + writtenByte - it_window.first,
                                    window[it_window.first].length() + it_window.first - 1 - (nextByte + writtenByte) -
       1);
                    }
                    window.erase(it_window.first);
                    unAssembledBytes -= writtenByte;
                    nextByte = nextByte + writtenByte;
                    break;
                } else {
                    if (eofSign && it_window.first + writtenByte >= eofByte) {
                        _output.end_input();
                        break;
                    }
                    unAssembledBytes -= writtenByte;
                    nextByte += writtenByte;
                }
            } else {
                break;
            }

        }
             for (size_t eraseKey : eraseKeys) {
                window.erase(eraseKey);
            }

    */

    // // when the capacity is full
    // if (windowsAssembledBytes + unAssembledBytes == _capacity) {
    //     return;
    //     }
    // // when there is spare room for incoming data
    
    // if (index + data.length() < nextByte) {
    //     return;
    // }
    // // index <= nextByte && index + data.length() >= nextByte
    // if (index <= nextByte) {
    //     size_t dataLen = data.length();
    //     size_t writtenBytes = _output.write(data);
    //     nextByte += writtenBytes;
    //     window[nextByte] = data.substr(writtenBytes, dataLen - writtenBytes);
    // } else {
    //     // index > nextByte, i.e. go to unAssembled session
    //     // if there is no overlapping data session
    //     auto it_tail = window.lower_bound(index + data.length() - 1);
    //     auto it_head = window.lower_bound(index);
    //     if ()
    //     if (it_head->first == index) {
    //         if (data.length() > window[it_head->first].length()) {
    //             unAssembledBytes += data.length() - window[it_head->first].length();
    //             window[it_head->first] = data;
    //         }
    //     } else {

    //     }

    //     if (it_tail->first == index + data.length() - 1) {
    //         if (_capacity - unAssembledBytes - windowsAssembledBytes < data.length()) {
    //             window[index] = data.substr(0, _capacity - unAssembledBytes - windowsAssembledBytes);
    //             unAssembledBytes += _capacity - unAssembledBytes - windowsAssembledBytes;
    //         } else {
    //             window[index] = data + window[it_tail->first];
    //             unAssembledBytes += data.length();
    //         }
    //     } else {
    //         size_t toIncludeDataLength =
    //             _capacity - unAssembledBytes - windowsAssembledBytes > data.length() 
    //                 ? data.length() 
    //                 : _capacity - unAssembledBytes - windowsAssembledBytes;
    //         window[index] = data.substr(0, toIncludeDataLength);
    //             }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes_num; }

bool StreamReassembler::empty() const { return _unassembled_bytes_num == 0; }

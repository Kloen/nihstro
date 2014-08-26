// Copyright 2014 Tony Wasserka
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the owner nor the names of its contributors may
//       be used to endorse or promote products derived from this software
//       without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "bit_field.h"

#pragma pack(1)

union Instruction {
    enum class OpCode : uint32_t {
        ADD   = 0x00,
        DP3   = 0x01,
        DP4   = 0x02,

        MUL   = 0x08,

        MAX   = 0x0C,
        MIN   = 0x0D,
        RCP   = 0x0E,
        RSQ   = 0x0F,

        MOV   = 0x13,

        RET   = 0x21,
        FLUSH = 0x22,

        CALL  = 0x24,
        CALL2 = 0x25,
        CALLC = 0x26,
        IFU   = 0x27,
        CMP   = 0x2E,
    };

    std::string GetOpCodeName() const {
        std::map<OpCode, std::string> map = {
            { OpCode::ADD, "ADD" },
            { OpCode::DP3, "DP3" },
            { OpCode::DP4, "DP4" },
            { OpCode::MUL, "MUL" },
            { OpCode::MAX, "MAX" },
            { OpCode::MIN, "MIN" },
            { OpCode::RCP, "RCP" },
            { OpCode::RSQ, "RSQ" },
            { OpCode::MOV, "MOV" },
            { OpCode::RET, "RET" },
            { OpCode::FLUSH, "FLS" },
            { OpCode::CALL, "CALL" },
            { OpCode::CMP, "CMP" },
        };
        auto it = map.find(opcode);
        if (it == map.end())
            return std::string("UNK") + std::to_string(static_cast<int>(opcode.Value()));
        else
            return it->second;
    }

    uint32_t hex;

    BitField<0x1a, 6, OpCode> opcode;

    // General notes:
    //
    // When two input registers are used, one of them uses a 5-bit index while the other
    // one uses a 7-bit index. This is because at most one floating point uniform may be used
    // as an input.


    // Format used e.g. by arithmetic instructions and comparisons
    union {
        BitField<0x00, 0x7, uint32_t> operand_desc_id;

        template<class BitFieldType>
        struct SourceRegister : BitFieldType {
            enum RegisterType {
                Input,
                Temporary,
                FloatUniform
            };

            RegisterType GetRegisterType() const {
                if (BitFieldType::Value() < 0x10)
                    return Input;
                else if (BitFieldType::Value() < 0x20)
                    return Temporary;
                else
                    return FloatUniform;
            }

            int GetIndex() const {
                if (GetRegisterType() == Input)
                    return BitFieldType::Value();
                else if (GetRegisterType() == Temporary)
                    return BitFieldType::Value() - 0x10;
                else if (GetRegisterType() == FloatUniform)
                    return BitFieldType::Value() - 0x20;
            }

            std::string GetRegisterName() const {
                std::map<RegisterType, std::string> type = {
                    { Input, "i" },
                    { Temporary, "t" },
                    { FloatUniform, "f" },
                };
                return type[GetRegisterType()] + std::to_string(GetIndex());
            }
        };

        SourceRegister<BitField<0x07, 0x5, uint32_t>> src2;
        SourceRegister<BitField<0x0c, 0x7, uint32_t>> src1;

        BitField<0x13, 0x2, uint32_t> unk2; // 3dbrew calls this FLAG

        struct : BitField<0x15, 0x5, uint32_t>
        {
            enum RegisterType {
                Output,
                Temporary,
                Unknown
            };

            RegisterType GetRegisterType() const {
                if (Value() < 0x8)
                    return Output;
                else if (Value() < 0x10)
                    return Unknown;
                else if (Value() < 0x20)
                    return Temporary;
                else
                    return Unknown;
            }

            int GetIndex() const {
                if (GetRegisterType() == Output)
                    return Value();
                else if (GetRegisterType() == Temporary)
                    return Value() - 0x10;
                else
                    return Value();
            }

            std::string GetRegisterName() const {
                std::map<RegisterType, std::string> type = {
                    { Output, "o" },
                    { Temporary, "t" },
                    { Unknown, "u" }
                };
                return type[GetRegisterType()] + std::to_string(GetIndex());
            }
        } dest;
    } common;

    // Format used for flow control instructions ("if")
    union {
        BitField<0x00, 0x8, uint32_t> num_instructions;
        BitField<0x0a, 0xc, uint32_t> offset_words;
    } flow_control;
};
static_assert(sizeof(Instruction) == 0x4, "Incorrect structure size");
static_assert(std::is_standard_layout<Instruction>::value, "Structure does not have standard layout");

union SwizzlePattern {
    uint32_t hex;

    enum class Selector : uint32_t {
        x = 0,
        y = 1,
        z = 2,
        w = 3
    };

    Selector GetSelectorSrc1(int comp) const {
        Selector selectors[] = {
            src1_selector_0, src1_selector_1, src1_selector_2, src1_selector_3
        };
        return selectors[comp];
    }

    Selector GetSelectorSrc2(int comp) const {
        Selector selectors[] = {
            src2_selector_0, src2_selector_1, src2_selector_2, src2_selector_3
        };
        return selectors[comp];
    }

    std::string SelectorToString(bool src2) const {
        std::map<Selector, std::string> map = {
            { Selector::x, "x" },
            { Selector::y, "y" },
            { Selector::z, "z" },
            { Selector::w, "w" }
        };
        std::string ret;
        for (int i = 0; i < 4; ++i) {
            ret += map.at(src2 ? GetSelectorSrc2(i) : GetSelectorSrc1(i));
        }
        return ret;
    }

    bool DestComponentEnabled(int i) const {
        return (dest_mask & (0x8 >> i));
    }

    std::string DestMaskToString() const {
        std::string ret;
        for (int i = 0; i < 4; ++i) {
            if (!DestComponentEnabled(i))
                ret += "_";
            else
                ret += "xyzw"[i];
        }
        return ret;
    }

    // Components of "dest" that should be written to: LSB=dest.w, MSB=dest.x
    BitField< 0, 4, uint32_t> dest_mask;

    BitField< 4, 1, uint32_t> negate_src1;

    BitField< 5, 2, Selector> src1_selector_3;
    BitField< 7, 2, Selector> src1_selector_2;
    BitField< 9, 2, Selector> src1_selector_1;
    BitField<11, 2, Selector> src1_selector_0;

    BitField<14, 2, Selector> src2_selector_3;
    BitField<16, 2, Selector> src2_selector_2;
    BitField<18, 2, Selector> src2_selector_1;
    BitField<20, 2, Selector> src2_selector_0;

    BitField<31, 1, uint32_t> flag; // not sure what this means, maybe it's the sign?
};
static_assert(sizeof(SwizzlePattern) == 0x4, "Incorrect structure size");


#pragma pack()

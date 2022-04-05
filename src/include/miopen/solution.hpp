/*******************************************************************************
 *
 * MIT License
 *
 * Copyright (c) 2022 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/

#pragma once

#include <miopen/miopen.h>

#include <miopen/errors.hpp>
#include <miopen/object.hpp>
#include <miopen/tensor.hpp>

#include <boost/optional.hpp>

namespace miopen {

struct Handle;

struct Solution : miopenSolution
{
    struct RunInput
    {
        miopenProblemTensorName_t name;
        boost::optional<TensorDescriptor> descriptor;
        void* buffer;
    };

    std::size_t GetSize() const;
    void Save(char* data) const;
    void Load(const char* data, std::size_t size);
    std::size_t GetTime() const { return time; }
    std::size_t GetWorkspaceSize() const { return workspace_size; }

    void Run(const Handle& handle,
             const std::vector<RunInput>& inputs,
             void* workspace,
             size_t workspaceSize);

private:
    std::size_t time;
    std::size_t workspace_size;
};

} // namespace miopen

inline std::ostream& operator<<(std::ostream& stream, const miopen::Solution& solution)
{
    // Todo: sane printing
    stream << &solution;
    return stream;
}

MIOPEN_DEFINE_OBJECT(miopenSolution, miopen::Solution);

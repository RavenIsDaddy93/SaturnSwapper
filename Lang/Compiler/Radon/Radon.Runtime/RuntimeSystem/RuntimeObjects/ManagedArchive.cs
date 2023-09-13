﻿using System;
using System.IO;
using Radon.CodeAnalysis.Emit;
using Radon.Runtime.Memory;
using UAssetAPI.IO;

namespace Radon.Runtime.RuntimeSystem.RuntimeObjects;

internal sealed class ManagedArchive : RuntimeObject
{
    public override RuntimeType Type { get; }
    public override int Size { get; } // The size in bytes of the array on the heap. This includes the 4 bytes for the length.
    public override nuint Address { get; } // The address of the array on the heap.
    public ZenAsset Archive { get; set; } // The archive

    public ManagedArchive(ZenAsset? archive, nuint pointer)
    {
        Archive = archive ?? new ZenAsset();
        Address = pointer;
        Type = ManagedRuntime.System.GetType("archive");
        Size = Type.Size;
    }

    public override RuntimeObject ComputeOperation(OpCode operation, RuntimeObject? other, StackFrame stackFrame)
    {
        if (other is not ManagedArchive otherArchive)
        {
            throw new InvalidOperationException("Cannot perform an operation on a archive and a non-archive.");
        }

        switch (operation)
        {
            case OpCode.Ceq:
            {
                return stackFrame.AllocatePrimitive(ManagedRuntime.Boolean, Address == otherArchive.Address);
            }
            case OpCode.Cne:
            {
                return stackFrame.AllocatePrimitive(ManagedRuntime.Boolean, Address != otherArchive.Address);
            }
        }

        throw new InvalidOperationException($"Cannot perform operation {operation} on a archive.");
    }

    public override RuntimeObject CopyTo(nuint address)
    {
        return new ManagedArchive(Archive, address);
    }

    public void Save()
    {
        File.WriteAllBytes(Path.Join(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Saturn", "SaturnPlugin.uasset"), Archive.WriteData().GetBuffer());
    }

    public override string ToString()
    {
        return Archive.Name.Value.Value;
    }
}
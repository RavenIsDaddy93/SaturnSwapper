using System.Runtime.InteropServices;

namespace Radon.CodeAnalysis.Emit.Binary.MetadataBinary;

[StructLayout(LayoutKind.Sequential, Pack = 1)]
internal readonly struct ConstantTable
{
    public readonly Constant[] Constants;
    
    public ConstantTable(Constant[] constants)
    {
        Constants = constants;
    }
}
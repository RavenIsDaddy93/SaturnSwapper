import Saturn.Structs.MappedName;

import Saturn.Readers.FArchive;

FArchive& operator<<(FArchive& Ar, FMappedName& MappedName)
{
	Ar << MappedName.Index << MappedName.Number;

	return Ar;
}
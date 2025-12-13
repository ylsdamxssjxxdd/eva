[CmdletBinding()]
param(
  [string]$CardNameLike = '*',
  [int]$SampleIntervalSeconds = 1,
  [int]$MaxSamples = 1,
  [ValidateSet('Max', 'Average')]
  [string]$UtilizationAggregate = 'Max',
  [bool]$All = $true,
  [switch]$RefreshCache,
  [switch]$NoCache,
  [switch]$NoDxDiag
)

$script:GpuStatusScriptRoot = if ($PSScriptRoot) {
  $PSScriptRoot
} elseif ($PSCommandPath) {
  Split-Path -Parent $PSCommandPath
} elseif ($MyInvocation -and $MyInvocation.MyCommand -and $MyInvocation.MyCommand.Path) {
  Split-Path -Parent $MyInvocation.MyCommand.Path
} else {
  (Get-Location).Path
}

$script:GpuStatusCachePath = Join-Path $script:GpuStatusScriptRoot 'Get-GpuStatus.cache.json'

function Get-GpuStatusCachePath {
  [CmdletBinding()]
  param()

  $script:GpuStatusCachePath
}

function Read-GpuStatusCache {
  [CmdletBinding()]
  param(
    [Parameter(Mandatory)]
    [string]$Path
  )

  if (-not (Test-Path $Path)) { return $null }
  try {
    $raw = Get-Content -Path $Path -Raw -ErrorAction Stop
    $cache = $raw | ConvertFrom-Json -ErrorAction Stop
    if (-not $cache) { return $null }
    if ($cache.CacheVersion -notin @(1, 2)) { return $null }
    return $cache
  } catch {
    return $null
  }
}

function Write-GpuStatusCache {
  [CmdletBinding()]
  param(
    [Parameter(Mandatory)]
    [string]$Path,
    [object[]]$DxDiagDevices,
    [object[]]$DxgiAdapters
  )

  $existing = Read-GpuStatusCache -Path $Path
  if (-not $PSBoundParameters.ContainsKey('DxDiagDevices')) {
    $DxDiagDevices = if ($existing -and $existing.DxDiagDevices) { @($existing.DxDiagDevices) } else { @() }
  }
  if (-not $PSBoundParameters.ContainsKey('DxgiAdapters')) {
    $DxgiAdapters = if ($existing -and $existing.DxgiAdapters) { @($existing.DxgiAdapters) } else { @() }
  }

  $DxDiagDevices = @($DxDiagDevices) | Where-Object { $_ }
  $DxgiAdapters = @($DxgiAdapters) | Where-Object { $_ }

  $cache = [pscustomobject]@{
    CacheVersion  = 2
    CreatedUtc    = (Get-Date).ToUniversalTime().ToString('o')
    ComputerName  = $env:COMPUTERNAME
    DxDiagDevices = $DxDiagDevices
    DxgiAdapters  = $DxgiAdapters
  }

  try {
    $json = $cache | ConvertTo-Json -Depth 10
    $tmp = "$Path.tmp"
    Set-Content -Path $tmp -Value $json -Encoding UTF8 -ErrorAction Stop
    Move-Item -Path $tmp -Destination $Path -Force -ErrorAction Stop
  } catch {
    Remove-Item -Path "$Path.tmp" -Force -ErrorAction SilentlyContinue
  }
}

function Get-DxDiagDisplayDevice {
  [CmdletBinding()]
  param(
    [string]$CachePath = (Get-GpuStatusCachePath),
    [switch]$RefreshCache,
    [switch]$NoCache,
    [switch]$NoDxDiag
  )

  if (-not $NoCache) {
    $cache = Read-GpuStatusCache -Path $CachePath
    if ($cache -and $cache.DxDiagDevices) {
      if ($NoDxDiag -or -not $RefreshCache) {
        return @($cache.DxDiagDevices)
      }
    }
  }

  if ($NoDxDiag) { return @() }

  $xmlPath = Join-Path $env:TEMP ("dxdiag_{0}.xml" -f [guid]::NewGuid().ToString('n'))

  try {
    try {
      Start-Process dxdiag -ArgumentList "/x `"$xmlPath`"" -Wait -WindowStyle Hidden -ErrorAction Stop | Out-Null
    } catch {
      return @()
    }
    if (-not (Test-Path $xmlPath)) { return @() }

    try {
      [xml]$x = Get-Content $xmlPath -Raw -Encoding UTF8
    } catch {
      return @()
    }
    $devices = @($x.DxDiag.DisplayDevices.DisplayDevice)

    $result = foreach ($d in $devices) {
      $deviceKey = [string]$d.DeviceKey
      $cardName = [string]$d.CardName

      $dedMB = [int64](($d.DedicatedMemory -replace '\D', ''))
      $shrMB = [int64](($d.SharedMemory -replace '\D', ''))
      $dispMB = [int64](($d.DisplayMemory -replace '\D', ''))

      $vendorId = $null
      $deviceId = $null
      $venDevKey = $null
      if ($deviceKey -match 'VEN_([0-9A-Fa-f]{4})&DEV_([0-9A-Fa-f]{4})') {
        $vendorId = $matches[1].ToLowerInvariant()
        $deviceId = $matches[2].ToLowerInvariant()
        $venDevKey = "ven_${vendorId}_dev_${deviceId}"
      }

      [pscustomobject]@{
        CardName       = $cardName
        DeviceKey      = $deviceKey
        VendorIdHex    = $vendorId
        DeviceIdHex    = $deviceId
        VenDevKey      = $venDevKey
        DedicatedMB    = $dedMB
        SharedMB       = $shrMB
        DisplayMB      = $dispMB
        DedicatedBytes = $dedMB * 1MB
      }
    }

    if (-not $NoCache -and $result) {
      Write-GpuStatusCache -Path $CachePath -DxDiagDevices @($result)
    }

    return @($result)
  } finally {
    Remove-Item $xmlPath -Force -ErrorAction SilentlyContinue
  }
}

function Get-DxgiAdapterRuntime {
  [CmdletBinding()]
  param()

  if (-not ('DxgiAdapterUtil' -as [type])) {
    $src = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public static class DxgiAdapterUtil
{
    [DllImport("dxgi.dll", CallingConvention = CallingConvention.StdCall)]
    private static extern int CreateDXGIFactory1(ref Guid riid, out IntPtr ppFactory);

    [ComImport]
    [Guid("770aae78-f26f-4dba-a829-253c83d1b387")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IDXGIFactory1
    {
        // IDXGIObject
        void SetPrivateData(ref Guid Name, uint DataSize, IntPtr pData);
        void SetPrivateDataInterface(ref Guid Name, IntPtr pUnknown);
        void GetPrivateData(ref Guid Name, ref uint pDataSize, IntPtr pData);
        void GetParent(ref Guid riid, out IntPtr ppParent);

        // IDXGIFactory
        void EnumAdapters(uint Adapter, out IntPtr ppAdapter);
        void MakeWindowAssociation(IntPtr WindowHandle, uint Flags);
        void GetWindowAssociation(out IntPtr WindowHandle);
        void CreateSwapChain(IntPtr pDevice, IntPtr pDesc, out IntPtr ppSwapChain);
        void CreateSoftwareAdapter(IntPtr Module, out IntPtr ppAdapter);

        // IDXGIFactory1
        void EnumAdapters1(uint Adapter, out IntPtr ppAdapter);
        bool IsCurrent();
    }

    [ComImport]
    [Guid("29038f61-3839-4626-91fd-086879011a05")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IDXGIAdapter1
    {
        // IDXGIObject
        void SetPrivateData(ref Guid Name, uint DataSize, IntPtr pData);
        void SetPrivateDataInterface(ref Guid Name, IntPtr pUnknown);
        void GetPrivateData(ref Guid Name, ref uint pDataSize, IntPtr pData);
        void GetParent(ref Guid riid, out IntPtr ppParent);

        // IDXGIAdapter
        void EnumOutputs(uint Output, out IntPtr ppOutput);
        void GetDesc(out DXGI_ADAPTER_DESC pDesc);
        void CheckInterfaceSupport(ref Guid InterfaceName, out long pUMDVersion);

        // IDXGIAdapter1
        void GetDesc1(out DXGI_ADAPTER_DESC1 pDesc);
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct LUID
    {
        public uint LowPart;
        public int HighPart;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct DXGI_ADAPTER_DESC
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
        public string Description;
        public uint VendorId;
        public uint DeviceId;
        public uint SubSysId;
        public uint Revision;
        public UIntPtr DedicatedVideoMemory;
        public UIntPtr DedicatedSystemMemory;
        public UIntPtr SharedSystemMemory;
        public LUID AdapterLuid;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct DXGI_ADAPTER_DESC1
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
        public string Description;
        public uint VendorId;
        public uint DeviceId;
        public uint SubSysId;
        public uint Revision;
        public UIntPtr DedicatedVideoMemory;
        public UIntPtr DedicatedSystemMemory;
        public UIntPtr SharedSystemMemory;
        public LUID AdapterLuid;
        public uint Flags;
    }

    public sealed class AdapterInfo
    {
        public string Description;
        public string Luid;
        public uint VendorId;
        public uint DeviceId;
        public ulong DedicatedVideoMemory;
        public uint Flags;
    }

    public static AdapterInfo[] Get()
    {
        var riid = typeof(IDXGIFactory1).GUID;
        IntPtr factoryPtr;
        int hr = CreateDXGIFactory1(ref riid, out factoryPtr);
        if (hr != 0) Marshal.ThrowExceptionForHR(hr);

        var factory = (IDXGIFactory1)Marshal.GetObjectForIUnknown(factoryPtr);
        try
        {
            var list = new List<AdapterInfo>();
            for (uint i = 0; ; i++)
            {
                IntPtr adapterPtr;
                try
                {
                    factory.EnumAdapters1(i, out adapterPtr);
                }
                catch (COMException ex)
                {
                    if ((uint)ex.HResult == 0x887A0002u) break; // DXGI_ERROR_NOT_FOUND
                    throw;
                }

                var adapter = (IDXGIAdapter1)Marshal.GetObjectForIUnknown(adapterPtr);
                try
                {
                    DXGI_ADAPTER_DESC1 desc;
                    adapter.GetDesc1(out desc);

                    string luid = string.Format(
                        "luid_0x{0:x8}_0x{1:x8}",
                        unchecked((uint)desc.AdapterLuid.HighPart),
                        desc.AdapterLuid.LowPart
                    );

                    list.Add(new AdapterInfo
                    {
                        Description = desc.Description,
                        Luid = luid,
                        VendorId = desc.VendorId,
                        DeviceId = desc.DeviceId,
                        DedicatedVideoMemory = desc.DedicatedVideoMemory.ToUInt64(),
                        Flags = desc.Flags
                    });
                }
                finally
                {
                    Marshal.ReleaseComObject(adapter);
                }
            }

            return list.ToArray();
        }
        finally
        {
            Marshal.ReleaseComObject(factory);
        }
    }
}
'@

    Add-Type -TypeDefinition $src -Language CSharp -ErrorAction Stop | Out-Null
  }

  [DxgiAdapterUtil]::Get()
}

function Get-DxgiAdapter {
  [CmdletBinding()]
  param(
    [string]$CachePath = (Get-GpuStatusCachePath),
    [switch]$RefreshCache,
    [switch]$NoCache
  )

  if (-not $NoCache) {
    $cache = Read-GpuStatusCache -Path $CachePath
    if ($cache -and $cache.DxgiAdapters -and -not $RefreshCache) {
      return @($cache.DxgiAdapters)
    }
  }

  $adapters = foreach ($a in @(Get-DxgiAdapterRuntime)) {
    if (-not $a) { continue }
    [pscustomobject]@{
      Description          = [string]$a.Description
      Luid                 = [string]$a.Luid
      VendorId             = [uint32]($a.VendorId -as [uint32])
      DeviceId             = [uint32]($a.DeviceId -as [uint32])
      Flags                = [uint32]($a.Flags -as [uint32])
      DedicatedVideoMemory = [uint64]($a.DedicatedVideoMemory -as [uint64])
    }
  }

  if (-not $NoCache -and $adapters) {
    Write-GpuStatusCache -Path $CachePath -DxgiAdapters @($adapters)
  }

  return @($adapters)
}

function Get-GpuStatus {
  [CmdletBinding()]
  param(
    [string]$CardNameLike = '*',
    [int]$SampleIntervalSeconds = 1,
    [int]$MaxSamples = 1,
    [ValidateSet('Max', 'Average')]
    [string]$UtilizationAggregate = 'Max',
    [bool]$All = $true,
    [switch]$RefreshCache,
    [switch]$NoCache,
    [switch]$NoDxDiag
  )

  $dxdiagByVenDev = @{}
  $dxdiagByName = @{}
  foreach ($d in (Get-DxDiagDisplayDevice -RefreshCache:$RefreshCache -NoCache:$NoCache -NoDxDiag:$NoDxDiag)) {
    if ($d.VenDevKey) {
      if (-not $dxdiagByVenDev.ContainsKey($d.VenDevKey) -or $d.DedicatedBytes -gt $dxdiagByVenDev[$d.VenDevKey].DedicatedBytes) {
        $dxdiagByVenDev[$d.VenDevKey] = $d
      }
    }
    if ($d.CardName) {
      $nameKey = $d.CardName.ToLowerInvariant()
      if (-not $dxdiagByName.ContainsKey($nameKey) -or $d.DedicatedBytes -gt $dxdiagByName[$nameKey].DedicatedBytes) {
        $dxdiagByName[$nameKey] = $d
      }
    }
  }

  $dxgiByLuidBase = @{}
  $realLuidBasesForEngineFilter = @()
  try {
    foreach ($a in (Get-DxgiAdapter -RefreshCache:$RefreshCache -NoCache:$NoCache)) {
      if (-not $a -or -not $a.Luid) { continue }

      $dxgiByLuidBase[$a.Luid.ToLowerInvariant()] = $a

      $flags = [int]($a.Flags -as [int])
      $isSoftware = (($flags -band 2) -ne 0)
      $isRemote = (($flags -band 4) -ne 0)
      $isMicrosoft = ([int]($a.VendorId -as [int]) -eq 0x1414)

      if ((-not $isSoftware) -and (-not $isRemote) -and (-not $isMicrosoft)) {
        $realLuidBasesForEngineFilter += $a.Luid.ToUpperInvariant()
      }
    }
  } catch {}

  $engineNameFilter = $null
  $uniqueRealLuidBases = @($realLuidBasesForEngineFilter | Sort-Object -Unique)
  if ($uniqueRealLuidBases.Count -gt 0) {
    $likeClauses = $uniqueRealLuidBases | ForEach-Object { "Name LIKE '%$_%'" }
    $engineNameFilter = '(' + ($likeClauses -join ' OR ') + ')'
  }

  $samplesToTake = if ($MaxSamples -lt 1) { 1 } else { $MaxSamples }
  $sleepSeconds = if ($SampleIntervalSeconds -lt 0) { 0 } else { $SampleIntervalSeconds }

  $cimSession = $null
  try {
    $so = New-CimSessionOption -Protocol Dcom
    $cimSession = New-CimSession -SessionOption $so
  } catch {
    $cimSession = $null
  }

  $adapterMemSample = $null
  $engineSample = $null

  try {
    for ($i = 1; $i -le $samplesToTake; $i++) {
      $adapterMemSample = @(
        if ($cimSession) {
          Get-CimInstance -CimSession $cimSession Win32_PerfFormattedData_GPUPerformanceCounters_GPUAdapterMemory -Property Name, DedicatedUsage -ErrorAction Stop
        } else {
          Get-CimInstance Win32_PerfFormattedData_GPUPerformanceCounters_GPUAdapterMemory -Property Name, DedicatedUsage -ErrorAction Stop
        }
      )

      if ($UtilizationAggregate -eq 'Max') {
        $wql = 'UtilizationPercentage > 0'
        if ($engineNameFilter) { $wql = "$wql AND $engineNameFilter" }
        try {
          $engineSample = @(
            if ($cimSession) {
              Get-CimInstance -CimSession $cimSession Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine -Property Name, UtilizationPercentage -Filter $wql -ErrorAction Stop
            } else {
              Get-CimInstance Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine -Property Name, UtilizationPercentage -Filter $wql -ErrorAction Stop
            }
          )
        } catch {
          try {
            $engineSample = @(
              if ($cimSession) {
                Get-CimInstance -CimSession $cimSession Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine -Property Name, UtilizationPercentage -Filter 'UtilizationPercentage > 0' -ErrorAction Stop
              } else {
                Get-CimInstance Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine -Property Name, UtilizationPercentage -Filter 'UtilizationPercentage > 0' -ErrorAction Stop
              }
            )
          } catch {
            $engineSample = @(
              if ($cimSession) {
                Get-CimInstance -CimSession $cimSession Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine -Property Name, UtilizationPercentage -ErrorAction Stop
              } else {
                Get-CimInstance Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine -Property Name, UtilizationPercentage -ErrorAction Stop
              }
            )
          }
        }
      } else {
        if ($engineNameFilter) {
          try {
            $engineSample = @(
              if ($cimSession) {
                Get-CimInstance -CimSession $cimSession Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine -Property Name, UtilizationPercentage -Filter $engineNameFilter -ErrorAction Stop
              } else {
                Get-CimInstance Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine -Property Name, UtilizationPercentage -Filter $engineNameFilter -ErrorAction Stop
              }
            )
          } catch {
            $engineSample = @(
              if ($cimSession) {
                Get-CimInstance -CimSession $cimSession Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine -Property Name, UtilizationPercentage -ErrorAction Stop
              } else {
                Get-CimInstance Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine -Property Name, UtilizationPercentage -ErrorAction Stop
              }
            )
          }
        } else {
          $engineSample = @(
            if ($cimSession) {
              Get-CimInstance -CimSession $cimSession Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine -Property Name, UtilizationPercentage -ErrorAction Stop
            } else {
              Get-CimInstance Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine -Property Name, UtilizationPercentage -ErrorAction Stop
            }
          )
        }
      }

      if ($i -lt $samplesToTake -and $sleepSeconds -gt 0) {
        Start-Sleep -Seconds $sleepSeconds
      }
    }
  } finally {
    if ($cimSession) {
      Remove-CimSession $cimSession -ErrorAction SilentlyContinue
    }
  }

  $mem = foreach ($s in $adapterMemSample) {
    [pscustomobject]@{
      Instance       = [string]$s.Name
      DedicatedBytes = [int64]$s.DedicatedUsage
    }
  }

  $utilByAdapterKey = @{}
  $utilSumByAdapterKey = @{}
  $utilCountByAdapterKey = @{}

  foreach ($s in $engineSample) {
    $name = [string]$s.Name
    $m = [regex]::Match($name, 'luid_0x[0-9A-Fa-f]+_0x[0-9A-Fa-f]+_phys_\d+')
    if (-not $m.Success) { continue }

    $v = [double]($s.UtilizationPercentage -as [double])
    if ($v -lt 0) { $v = 0 }
    if ($v -gt 100) { $v = 100 }

    $k = $m.Value
    if ($UtilizationAggregate -eq 'Max') {
      if (-not $utilByAdapterKey.ContainsKey($k) -or $v -gt $utilByAdapterKey[$k]) {
        $utilByAdapterKey[$k] = $v
      }
    } else {
      if (-not $utilSumByAdapterKey.ContainsKey($k)) {
        $utilSumByAdapterKey[$k] = 0.0
        $utilCountByAdapterKey[$k] = 0
      }
      $utilSumByAdapterKey[$k] += $v
      $utilCountByAdapterKey[$k] += 1
    }
  }

  if ($UtilizationAggregate -eq 'Max') {
    foreach ($k in @($utilByAdapterKey.Keys)) {
      $utilByAdapterKey[$k] = [math]::Round([double]$utilByAdapterKey[$k], 2)
    }
  } else {
    foreach ($k in @($utilSumByAdapterKey.Keys)) {
      $count = [double]$utilCountByAdapterKey[$k]
      $avg = if ($count -gt 0) { $utilSumByAdapterKey[$k] / $count } else { 0.0 }
      $utilByAdapterKey[$k] = [math]::Round([double]$avg, 2)
    }
  }

  $results = foreach ($m in $mem) {
    $luidBase = [regex]::Match($m.Instance, '^luid_0x[0-9A-Fa-f]+_0x[0-9A-Fa-f]+').Value
    $adapterKey = [regex]::Match($m.Instance, '^luid_0x[0-9A-Fa-f]+_0x[0-9A-Fa-f]+_phys_\d+').Value

    $dxgi = $null
    if ($luidBase) { $dxgi = $dxgiByLuidBase[$luidBase.ToLowerInvariant()] }

    $name = $null
    if ($dxgi -and $dxgi.Description) { $name = $dxgi.Description }

    $isRealGpu = $false
    if ($dxgi) {
      $flags = [int]($dxgi.Flags -as [int])
      $isSoftware = (($flags -band 2) -ne 0)
      $isRemote = (($flags -band 4) -ne 0)
      $isMicrosoft = ([int]$dxgi.VendorId -eq 0x1414)
      $isRealGpu = (-not $isSoftware) -and (-not $isRemote) -and (-not $isMicrosoft)
    }

    $totalBytes = $null
    $totalSource = $null

    if ($dxgi) {
      $venDevKey = ('ven_{0:x4}_dev_{1:x4}' -f $dxgi.VendorId, $dxgi.DeviceId).ToLowerInvariant()
      if ($dxdiagByVenDev.ContainsKey($venDevKey)) {
        $totalBytes = [int64]($dxdiagByVenDev[$venDevKey].DedicatedBytes -as [double])
        if (-not $name) { $name = $dxdiagByVenDev[$venDevKey].CardName }
        $totalSource = 'DxDiag DedicatedMemory'
      }
    }

    if ($null -eq $totalBytes -and $name) {
      $nameKey = $name.ToLowerInvariant()
      if ($dxdiagByName.ContainsKey($nameKey)) {
        $totalBytes = [int64]($dxdiagByName[$nameKey].DedicatedBytes -as [double])
        $totalSource = 'DxDiag DedicatedMemory (name match)'
      }
    }

    if ($null -eq $totalBytes -and $dxgi -and $dxgi.DedicatedVideoMemory -gt 0) {
      $totalBytes = [int64]$dxgi.DedicatedVideoMemory
      $totalSource = 'DXGI DedicatedVideoMemory'
    }

    $freeBytes = $null
    if ($null -ne $totalBytes) {
      $freeBytes = $totalBytes - $m.DedicatedBytes
      if ($freeBytes -lt 0) { $freeBytes = 0 }
    }

    $util = $null
    if ($adapterKey -and $utilByAdapterKey.ContainsKey($adapterKey)) { $util = $utilByAdapterKey[$adapterKey] } else { $util = 0 }

    [pscustomobject]@{
      Instance         = $m.Instance
      Name             = $name
      TotalBytes       = $totalBytes
      UsedBytes        = [int64]$m.DedicatedBytes
      FreeBytes        = $freeBytes
      UtilizationPct   = $util
      TotalSource      = $totalSource
      IsRealGpuAdapter = $isRealGpu
    }
  }

  $real = $results | Where-Object { $_.IsRealGpuAdapter }
  if ($CardNameLike -and $CardNameLike -ne '*') {
    $real = $real | Where-Object { $_.Name -like $CardNameLike }
  }

  $real = $real | Sort-Object UsedBytes -Descending

  [int64]$sumTotalBytes = 0
  [int64]$sumUsedBytes = 0
  [int64]$sumFreeBytes = 0

  $utilValues = @()
  foreach ($g in $real) {
    $sumUsedBytes += [int64]$g.UsedBytes

    if ($null -ne $g.TotalBytes) { $sumTotalBytes += [int64]$g.TotalBytes }
    if ($null -ne $g.FreeBytes) { $sumFreeBytes += [int64]$g.FreeBytes }

    if ($null -ne $g.UtilizationPct) { $utilValues += [double]$g.UtilizationPct }
  }

  $summaryUtil = $null
  if ($utilValues.Count -gt 0) {
    $summaryUtil = if ($UtilizationAggregate -eq 'Average') {
      [math]::Round([double](($utilValues | Measure-Object -Average).Average), 2)
    } else {
      [math]::Round([double](($utilValues | Measure-Object -Maximum).Maximum), 2)
    }
  }

  $detail = $real | ForEach-Object {
    [pscustomobject]@{
      Name           = $_.Name
      TotalGB        = if ($null -ne $_.TotalBytes) { [math]::Round($_.TotalBytes / 1GB, 2) } else { $null }
      UsedGB         = [math]::Round($_.UsedBytes / 1GB, 2)
      FreeGB         = if ($null -ne $_.FreeBytes) { [math]::Round($_.FreeBytes / 1GB, 2) } else { $null }
      UtilizationPct = $_.UtilizationPct
    }
  }

  if ($All) {
    $detailText = $detail | Format-Table | Out-String -Width 200
    if ($detailText) { Write-Host $detailText }
  }

  [pscustomobject]@{
    TotalGB        = [math]::Round($sumTotalBytes / 1GB, 2)
    UsedGB         = [math]::Round($sumUsedBytes / 1GB, 2)
    FreeGB         = [math]::Round($sumFreeBytes / 1GB, 2)
    UtilizationPct = $summaryUtil
  }
}

Get-GpuStatus -CardNameLike $CardNameLike -SampleIntervalSeconds $SampleIntervalSeconds -MaxSamples $MaxSamples -UtilizationAggregate $UtilizationAggregate -All:$All -RefreshCache:$RefreshCache -NoCache:$NoCache -NoDxDiag:$NoDxDiag

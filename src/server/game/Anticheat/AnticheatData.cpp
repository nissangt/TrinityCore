#include "AnticheatData.h"

AnticheatData::AnticheatData() : _lastOpcode(0), _hasDailyReport(false)
{
    Reset();
}

AnticheatData::~AnticheatData() { }

void AnticheatData::Reset()
{
    _totalReports = 0;
    _average = 0.0f;
    _creationTime = 0;
    memset(&_typeReports, 0, sizeof(_typeReports[0]) * MAX_REPORT_TYPES);
    memset(&_tempReports, 0, sizeof(_tempReports[0]) * MAX_REPORT_TYPES);
    memset(&_tempReportsTimer, 0, sizeof(_tempReportsTimer[0]) * MAX_REPORT_TYPES);
}

void  AnticheatData::SaveToDB(const char* tableName, uint32 guidLow)
{
    CharacterDatabase.PExecute("REPLACE INTO %s(guid,average,total_reports,speed_reports,fly_reports,jump_reports,waterwalk_reports,teleportplane_reports,climb_reports,creation_time) VALUES (%u,%f,%u,%u,%u,%u,%u,%u,%u,%u)",
                               tableName, guidLow,
                               _average, _totalReports, 
                               _typeReports[SPEED_HACK_REPORT],
                               _typeReports[FLY_HACK_REPORT],
                               _typeReports[JUMP_HACK_REPORT],
                               _typeReports[WALK_WATER_HACK_REPORT],
                               _typeReports[TELEPORT_PLANE_HACK_REPORT],
                               _typeReports[CLIMB_HACK_REPORT],
                               _creationTime);
}

void AnticheatData::SetDailyReportState(bool b)
{
    _hasDailyReport = b;
}

bool AnticheatData::GetDailyReportState()
{
    return _hasDailyReport;
}

void AnticheatData::SetLastOpcode(uint32 opcode)
{
    _lastOpcode = opcode;
}

void AnticheatData::SetPosition(float x, float y, float z, float o)
{
    _lastMovementInfo.pos.m_positionX = x;
    _lastMovementInfo.pos.m_positionY = y;
    _lastMovementInfo.pos.m_positionZ = z;
    _lastMovementInfo.pos.m_orientation = o;
}

uint32 AnticheatData::GetLastOpcode() const
{
    return _lastOpcode;
}

const MovementInfo& AnticheatData::GetLastMovementInfo() const
{
    return _lastMovementInfo;
}

void AnticheatData::SetLastMovementInfo(MovementInfo& moveInfo)
{
    _lastMovementInfo = moveInfo;
}

uint32 AnticheatData::GetTotalReports() const
{
    return _totalReports;
}

void AnticheatData::SetTotalReports(uint32 totalReports)
{
    _totalReports = totalReports;
}

void AnticheatData::SetTypeReports(uint32 type, uint32 amount)
{
    _typeReports[type] = amount;
}

uint32 AnticheatData::GetTypeReports(uint32 type) const
{
    return _typeReports[type];
}

float AnticheatData::GetAverage() const
{
    return _average;
}

void AnticheatData::SetAverage(float average)
{
    _average = average;
}

uint32 AnticheatData::GetCreationTime() const
{
    return _creationTime;
}

void AnticheatData::SetCreationTime(uint32 creationTime)
{
    _creationTime = creationTime;
}

void AnticheatData::SetTempReports(uint8 type, uint32 amount)
{
    _tempReports[type] = amount;
}

uint32 AnticheatData::GetTempReports(uint8 type)
{
    return _tempReports[type];
}

void AnticheatData::SetTempReportsTimer(uint8 type, uint32 time)
{
    _tempReportsTimer[type] = time;
}

uint32 AnticheatData::GetTempReportsTimer(uint8 type)
{
    return _tempReportsTimer[type];
}

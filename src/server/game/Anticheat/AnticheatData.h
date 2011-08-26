#ifndef SC_ACDATA_H
#define SC_ACDATA_H

#include "AnticheatMgr.h"

class AnticheatData
{
public:
    AnticheatData();
    ~AnticheatData();

    void Reset();
    void SaveToDB(const char* tableName, uint32 guidLow);

    void SetLastOpcode(uint32 opcode);
    uint32 GetLastOpcode() const;

    const MovementInfo& GetLastMovementInfo() const;
    void SetLastMovementInfo(MovementInfo& moveInfo);

    void SetPosition(float x, float y, float z, float o);

    uint32 GetTotalReports() const;
    void SetTotalReports(uint32 totalReports);

    uint32 GetTypeReports(uint32 type) const;
    void SetTypeReports(uint32 type, uint32 amount);

    float GetAverage() const;
    void SetAverage(float average);

    uint32 GetCreationTime() const;
    void SetCreationTime(uint32 creationTime);

    void SetTempReports(uint8 type, uint32 amount);
    uint32 GetTempReports(uint8 type);

    void SetTempReportsTimer(uint8 type, uint32 time);
    uint32 GetTempReportsTimer(uint8 type);
    
    void SetDailyReportState(bool b);
    bool GetDailyReportState();

private:
    uint32          _guidLow;
    uint32          _lastOpcode;
    MovementInfo    _lastMovementInfo;
    uint32          _totalReports;
    float           _average;
    uint32          _creationTime;
    uint32          _typeReports[MAX_REPORT_TYPES];
    uint32          _tempReports[MAX_REPORT_TYPES];
    uint32          _tempReportsTimer[MAX_REPORT_TYPES];
    bool            _hasDailyReport;
};

#endif

*----------------------------------------------------------------------------*/
/* Copyright (c) 2018 FIRST. All Rights Reserved.                             */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/
#include "Limelight.h"

Limelight::Limelight() : 
    table{nt::NetworkTableInstance::GetDefault().GetTable("limelight")} itable->PutNumber("ledMode", 3); // force LEDs on
    table->PutNumber("camMode", 0); // vision processor
}

void Limelight::SetDriveCamMode() {
    table->PutNumber("ledMode", 1); // force LEDs off
    table->PutNumber("camMode", 1); // drive cam (increases exposure, disables vision processing)
}

LimelightValues Limelight::GetInfo() {
     auto t = Penguin::Constants::LIMELIGHT_DEFAULT_VALUE;

  return {table->GetNum         table->GetNumber("ty", t)
("tx", t),

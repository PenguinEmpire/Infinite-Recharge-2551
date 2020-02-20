/*----------------------------------------------------------------------------*/
/* Copyright (c) 2018 FIRST. All Rights Reserved.                             */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/

#include <wpi/math>
#include "frc/smartdashboard/SmartDashboard.h"

#include <units/units.h>

#include "SwerveModule.h"

SwerveModule::SwerveModule(frc::Translation2d pos, int analogEncoderPort, units::radian_t analogEncoderOffset, int driveMotorCANID, int turnMotorCANID, SwerveModuleName moduleName)
  : m_driveMotor(driveMotorCANID, rev::CANSparkMax::MotorType::kBrushless), 
    m_turnMotor(turnMotorCANID, rev::CANSparkMax::MotorType::kBrushless),
    m_moduleName{moduleName},
    m_turnEncoder{analogEncoderPort, analogEncoderOffset, m_turnMotor.GetEncoder()},
    m_modulePosition{pos} {

  // TODO: Figure out why this broke it
  // m_turnMotor.RestoreFactoryDefaults();
  // m_driveMotor.RestoreFactoryDefaults();
   
  // TODO: current limits.
  // m_turnMotor.SetSmartCurrentLimit();
  // m_driveMotor.SetSmartCurrentLimit();
  // m_turnMotor.SetSecondaryCurrentLimit();
  // m_driveMotor.SetSecondaryCurrentLimit();

  m_driveEncoder.SetPositionConversionFactor(SDS_WHEEL_DIAMETER * PenguinUtil::PI / SDS_DRIVE_REDUCTION); // so this is meters, right? // or, I guess inches? (wheel diameter is inches, conversion multiplier is circumference/reduction) huh.
  m_driveEncoder.SetVelocityConversionFactor(SDS_WHEEL_DIAMETER * PenguinUtil::PI / SDS_DRIVE_REDUCTION * (1.0 / 60.0)); // SDS: "RPM to units per sec"

  m_onboardTurnMotorPIDController.SetP(1.5);
  m_onboardTurnMotorPIDController.SetI(0);
  m_onboardTurnMotorPIDController.SetD(0.5);

  ReadSensors();
}

void SwerveModule::PutDiagnostics() {
  using SD = frc::SmartDashboard;

  SD::PutNumber(m_moduleName.GetAbbrUpper() + " angle (analog)", units::radian_t(m_currentAngle).to<double>());
  SD::PutNumber(m_moduleName.GetAbbrUpper() + " angle (analog) (cont)", m_turnEncoder.GetAngle(true).to<double>());
  SD::PutNumber(m_moduleName.GetAbbrUpper() + " angleMotorEncoder", m_turnEncoder.GetMotorEncoderPosition().to<double>());
  // SD::PutNumber(m_moduleName.GetAbbrUpper() + " driveMotorEncoderPosition", m_driveEncoder.GetPosition());
  // SD::PutNumber(m_moduleName.GetAbbrUpper() + " driveMotorEncoderVelocity", m_driveEncoder.GetVelocity());

  SD::PutString(m_moduleName.GetAbbrUpper() + " z-----sep", "");

}

/** Deprecated. Worked tho! */
void SwerveModule::Solve180Problem7_CenterOnCurrent(frc::SwerveModuleState& state) {
  PutSwerveModuleState("(7.1) pre-norm", state);

  units::meters_per_second_t targetSpeed = state.speed;
  frc::Rotation2d targetAngle = state.angle;

  units::radian_t currentAngle = m_turnEncoder.GetMotorEncoderPosition();
  // units::radian_t currentAngle = m_turnEncoder.GetAngle(false);

  units::radian_t targetAngle_r = targetAngle.Radians();

  // Here's the part that does the 180 problem
  // units::radian_t delta = PenguinUtil::arbitraryTwoPiRangeNorm(currentAngle - targetAngle_r, 0_rad, PenguinUtil::TWO_PI_RAD);
  // units::radian_t delta = currentAngle - targetAngle_r;
  // delta = units::math::fmod(delta, 180_deg);
  // if (delta < 0_deg) {
  //   delta += 180_deg;
  // }
  units::radian_t delta = 180_deg - units::math::abs(units::math::fmod(units::math::abs(currentAngle - targetAngle_r), 360_deg) - 180_deg); // Credit to ruahk on StackOverflow: https://stackoverflow.com/a/9505991
  // units::radian_t delta = PenguinUtil::piNegPiNorm(currentAngle - targetAngle_r); // not sure
  if (delta > 90_deg) { 
    targetAngle_r += 180_deg;
    targetSpeed *= -1;
  }

  // Here's the part that puts that in the right range  
  targetAngle_r = PenguinUtil::arbitraryTwoPiRangeNorm(targetAngle_r, currentAngle - PenguinUtil::PI_RAD, currentAngle + PenguinUtil::PI_RAD);

  SetDirectly(targetAngle_r.to<double>(), targetSpeed.to<double>());

  // Assign back
  state.speed = targetSpeed;
  state.angle = targetAngle;

  // PutSwerveModuleState("(7.2) post-norm", state);
}

/** Deprecated. Implemented in `SetDesiredState` now. */
void SwerveModule::Solve180Problem10_CenterUsingRotation(frc::SwerveModuleState& state) {
  PutSwerveModuleState("(10.1) pre-norm", state);

  // start with what we're given
  units::meters_per_second_t targetSpeed = state.speed;
  units::radian_t targetAngle = state.angle.Radians();
  units::radian_t currentAngle = m_turnEncoder.GetMotorEncoderPosition();
  // units::radian_t currentAngle = m_turnEncoder.GetAngle(true); // use built-in or analog encoder?

  // 180 Problem
  units::radian_t delta = 180_deg - units::math::abs(units::math::fmod(units::math::abs(currentAngle - targetAngle), 360_deg) - 180_deg); // Credit to ruahk on StackOverflow: https://stackoverflow.com/a/9505991
  if (delta > 90_deg) { 
    targetAngle += 180_deg;
    targetSpeed *= -1;
  }

  // Put it in the correct range. Probably this works?
  // targetAngle = currentAngle + units::math::fmod(targetAngle, 180_deg);
  targetAngle = PenguinUtil::arbitraryTwoPiRangeNorm2(targetAngle, currentAngle);

  PutSwerveModuleState("values we really want", targetAngle.to<double>(), targetSpeed.to<double>());

  // Assign back
  state.speed = targetSpeed;
  state.angle = frc::Rotation2d(targetAngle);

  PutSwerveModuleState("(10.2) post-norm", state);
}

void SwerveModule::SetDesiredState(frc::SwerveModuleState& state) {
  PutSwerveModuleState("requested", state);

  // Start with what we're given
  units::meters_per_second_t targetSpeed = state.speed;
  units::radian_t targetAngle = state.angle.Radians();
  units::radian_t currentAngle = m_turnEncoder.GetMotorEncoderPosition();
  // units::radian_t currentAngle = m_turnEncoder.GetAngle(true); // use built-in or analog encoder?

  // 'Turn 180 or drive backwards?'
  units::radian_t delta = 180_deg - units::math::abs(units::math::fmod(units::math::abs(currentAngle - targetAngle), 360_deg) - 180_deg); // Credit to ruahk on StackOverflow: https://stackoverflow.com/a/9505991
  if (delta > 90_deg) { 
    targetAngle += 180_deg;
    targetSpeed *= -1;
  }

  // Put it in the correct range.
  // targetAngle = currentAngle + units::math::fmod(targetAngle, 180_deg); //  Probably this works? EDIT: no it doesn't
  targetAngle = PenguinUtil::arbitraryTwoPiRangeNorm2(targetAngle, currentAngle);

  // Send it to the motor
  double toMotorAngle = targetAngle.to<double>();
  double toMotorSpeed = targetSpeed.to<double>();

  PutSwerveModuleState("true to-motor", toMotorAngle, toMotorSpeed);

  SetDirectly(toMotorAngle, toMotorSpeed);
}

void SwerveModule::SetDirectly(double angle, double speed) {
  m_driveMotor.Set(speed);
  m_onboardTurnMotorPIDController.SetReference(angle, rev::ControlType::kPosition);
}

void SwerveModule::ReadSensors() {
  /* Original discussion when I was porting of the four different variable writes in this function, and where each of them are coming from/going to

    SDS_currentAngle = m_currentAngle.to<double>();
      // use in steeringMotor.set(angleController.calculate(getCurrentAngle(), dt)); -- dt is .02 seconds
      // 


    // SDS_currentDistance = ReadDistance();
      // driveEncoder.GetPosition() (with the position conversion factor set to (wheelDiameter * Math.PI / reduction))
      // driveEncoder.GetPosition() * (1.0 / SDS_DEFAULT_DRIVE_ROTATIONS_PER_UNIT)
      // pretty sure these are not the same, and I don't know which one wins
      // currentDistance is used in `updateKinematics` to update `currentPosition`, but afaict `currentPosition` isn't used anywhere???

    // SDS_currentDraw = ReadCurrentDraw();
      // driveMotor.getOutputCurrent()
      // I think also not used anywhere??
    // SDS_velocity = ReadVelocity();
      // driveEncoder.GetVelocity() (with the velocity conversion factor set to (wheelDiameter * Math.PI / reduction * (1.0 / 60.0)) )
      // I think also not used anywhere?
  */

  m_currentAngle = m_turnEncoder.GetAngle(); // m_currentAngle is only set here, and always means this
  // SDS_currentDistance2 = SDS_ReadDistance(); // `SDS_ReadDistance` was `m_driveEncoder.GetPosition()`
}

void SwerveModule::PutSwerveModuleState(std::string info, frc::SwerveModuleState& state) {
  double speed_ = state.speed.to<double>();
  double angle_ = state.angle.Radians().to<double>();

  frc::SmartDashboard::PutNumber(m_moduleName.GetAbbrUpper() + " " + info + " speed", speed_);
  frc::SmartDashboard::PutNumber(m_moduleName.GetAbbrUpper() + " " + info + " angle", angle_);
}

void SwerveModule::PutSwerveModuleState(std::string info, units::degree_t angle, units::meters_per_second_t speed) {
  PutSwerveModuleState(info, angle.to<double>(), speed.to<double>());
}


void SwerveModule::PutSwerveModuleState(std::string info, double angle, double speed) {
  frc::SmartDashboard::PutNumber(m_moduleName.GetAbbrUpper() + " raw " + info + " speed", speed);
  frc::SmartDashboard::PutNumber(m_moduleName.GetAbbrUpper() + " raw " + info + " angle", angle);
}

void SwerveModule::UpdateAnalogOffset() {
  m_turnEncoder.UpdateOffset();
}
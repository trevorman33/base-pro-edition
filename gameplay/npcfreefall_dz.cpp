
#include "headers.h"
#include "npcfreefall_dz.h"
#include "npcmove.h"

/**
 * class implementation
 */

NPCFreefall_DZ::NPCFreefall_DZ(NPC* npc) : NPCProgram( npc )
{
    Matrix4f catToyPose = npc->getCatToy()->getCurrentPose();
    _targetPos.set( catToyPose[3][0], catToyPose[3][1], catToyPose[3][2] );
    _positionIsSucceed = false;
    _directionIsSucceed = false;
	_timeUntilJump = getCore()->getRandToolkit()->getUniform(1.0f, 10.0f);
}

NPCFreefall_DZ::~NPCFreefall_DZ()
{
}

/**
 * abstraction layer
 */

void NPCFreefall_DZ::onUpdate(float dt)
{
    // obtain spinal cord
    SpinalCord* spinalCord = getNPC()->getSpinalCord();
    spinalCord->reset();

    // update target position
    Matrix4f catToyPose = getNPC()->getCatToy()->getCurrentPose();
    _targetPos.set( catToyPose[3][0], catToyPose[3][1], catToyPose[3][2] );

	// jump only when the wind is right
	Vector2f pos = Vector2f(catToyPose[3][0], catToyPose[3][2]);
	pos.normalize();
	
	NxVec3 wind = getNPC()->getJumper()->getScene()->getWindAtPoint(NxVec3(catToyPose[3][0], catToyPose[3][1], catToyPose[3][2]));
	wind.normalize();
	Vector2f wind2(wind.x, wind.z);
	float wind_angle = 0.0;
	if (wind.magnitude() < 1.5f) {
		wind_angle = 1.0f;
	} else {
		wind2.normalize();
		wind_angle = pos.dot(wind2);
	}


    // if my character is still roaming
	if (wind_angle >= 0.8f && catToyPose[3][1] >= 300.0f) _timeUntilJump -= dt;
	//getCore()->logMessage("%s has %2.4f seconds to jump", getNPC()->getNPCName(), _timeUntilJump);

	if( _timeUntilJump <= 0.0f && getNPC()->getJumper()->getPhase() == ::jpRoaming )
    {
		Vector3f pos = Vector3f(catToyPose[3][0], catToyPose[3][1], catToyPose[3][2]);
        // if npc on airplane
        if( getNPC()->getJumper()->getAirplane() != NULL )
        {
            // just jump!
            spinalCord->phase = true;
            return;
        }
        // if npc at the exit point and ready to jump        
        else if( _positionIsSucceed && _directionIsSucceed )
        {
            // jump!
            spinalCord->phase = true;
            return;            
        }
        // if npc at the exit point and not surely ready to clear jump
        else
        {
            if( !_positionIsSucceed )
            {
                // move to abyss
                Matrix4f jumpPose = getNPC()->getCatToy()->getJumpPose();
                Vector3f jumpPos( jumpPose[3][0], jumpPose[3][1], jumpPose[3][2] );
                call( new NPCMove( getNPC(), jumpPos ) );
                _positionIsSucceed = true;
                return;
            }
            if( !_directionIsSucceed )
            {
                // jumper absolute orientation
                Vector3f jumperAt = getNPC()->getJumper()->getClump()->getFrame()->getAt();
                jumperAt.normalize();

                // direction of cat toy jump
                Matrix4f jumpPose = getNPC()->getCatToy()->getJumpPose();
                Vector3f targetDir( jumpPose[2][0], 0.0f, jumpPose[2][2] );
                targetDir.normalize();

                // angle to target
                Vector3f atH = jumperAt; atH[1] = 0; atH.normalize();
                Vector3f dirH = targetDir; dirH[1] = 0; dirH.normalize();
                float targetAngle = ::calcAngle( dirH, atH, Vector3f( 0,1,0 ) );

                if( fabs( targetAngle ) < 1.0f )
                {
                    _directionIsSucceed = true;
                    return;
                }
                else
                {
                    // turn jumper by AI algo
                    float aiRotationVel = 180.0f;
                    float aiRotationAngle = sgn( targetAngle ) * aiRotationVel * dt;
                    if( fabs( aiRotationAngle ) > fabs( targetAngle ) ) aiRotationAngle = targetAngle;
                    getNPC()->getJumper()->getClump()->getFrame()->rotateRelative( Vector3f( 0,1,0 ), aiRotationAngle );
                }
            }
        }
    }
    // else, turn & track towards the target
    else if( getNPC()->getJumper()->getPhase() == ::jpFreeFalling )
    {
         // jumper absolute orientation
        Vector3f jumperAt = getNPC()->getJumper()->getClump()->getFrame()->getAt();
        jumperAt.normalize();

        Vector3f jumperRight = getNPC()->getJumper()->getClump()->getFrame()->getRight();
        jumperRight.normalize();

        // retrieve current jumper position    
        Vector3f jumperPos = getNPC()->getJumper()->getClump()->getFrame()->getPos();
        jumperPos += Vector3f( 0, jumperRoamingSphereSize, 0 );

        // direction to target
        Vector3f targetDir = _targetPos - jumperPos;
        float distanceToTarget = Vector3f( targetDir[0], 0, targetDir[2] ).length();
        targetDir.normalize();

		// landing too far away? let's deploy in order to return safely
		// glide ratio is based on canopy size
		database::Canopy *canopy = database::Canopy::getRecord(getNPC()->getJumper()->getVirtues()->equipment.canopy.id);
		Vector3f pos = getNPC()->getJumper()->getClump()->getFrame()->getPos();
		float glide = canopy->square / 150.0f;
		float alt = pos[1];
		pos[1] = 0;
		bool farEnough = pos.length() >= alt*glide;
		//getCore()->logMessage("alt: %2.5f; dst: %2.5f; coverage: %2.5f", alt, pos.length(), alt*glide);
		// deploy now?
		if (alt <= 75000.0f || farEnough) {
			spinalCord->phase = true;
			//getCore()->logMessage("npc pull. Far enough: %d", (int)farEnough);
		}

        // if toy tracking modifier is on
		bool wingsuit = database::Suit::getRecord(getNPC()->getJumper()->getVirtues()->equipment.suit.id)->wingsuit;
		if( wingsuit /* getNPC()->getCatToy()->getModifier()*/ )
        {
            // sum up target direction 
            Vector3f targetFlatAt(
                getNPC()->getCatToy()->getCurrentPose()[2][0],
                0,
                getNPC()->getCatToy()->getCurrentPose()[2][2]
            );
            /*getCore()->logMessage(                 
                "POS: %3.2f %3.2f %3.2f AT : %3.2f 0.0 %3.2f", 
                getNPC()->getCatToy()->getCurrentPose()[3][0],
                getNPC()->getCatToy()->getCurrentPose()[3][1],
                getNPC()->getCatToy()->getCurrentPose()[3][2],
                targetFlatAt[0], targetFlatAt[2] 
            );*/
            targetFlatAt.normalize();
            targetDir += targetFlatAt * 3;
            targetDir.normalize();
        }

        // angle to target
        Vector3f atH = jumperAt; atH[1] = 0; atH.normalize();
        Vector3f dirH = targetDir; dirH[1] = 0; dirH.normalize();
        float targetAngle = ::calcAngle( dirH, atH, Vector3f( 0,1,0 ) );

        // inclination angle relative to the horizont
        float horizontalAngle = -1 * ::calcAngle( atH, jumperAt, jumperRight );

        // horizontal steering
        float minAngle = 5.0f;
        float minValue = 0.0f;
        float maxAngle = 45.0f;
        float maxValue = 1.0f;
        float factor = ( fabs( targetAngle ) - minAngle ) / ( maxAngle - minAngle );
        factor = ( factor < 0 ) ? 0 : ( ( factor > 1 ) ? 1 : factor );
        float impulse = minValue * ( 1 - factor ) + maxValue * factor;
        // smooth impulse
        impulse = pow( impulse, 1.25f );
        // apply impulse
        if( targetAngle < 0 )
        {            
            spinalCord->right = impulse;
        }
        else
        {
            spinalCord->left = impulse;
        }
       
        // relation btw desired inclination and target angle
        minAngle = 5.0f;
        minValue = 25.0f;
        maxAngle = 90.0f;
        maxValue = 0.0f;
        factor = ( fabs( targetAngle ) - minAngle ) / ( maxAngle - minAngle );
        factor = ( factor < 0 ) ? 0 : ( ( factor > 1 ) ? 1 : factor );
        float desiredInclination = minValue * ( 1 - factor ) + maxValue * factor;

        // inclination difference
        float inclinationDifference = desiredInclination - horizontalAngle;

        // vertical steering
        minAngle = 0.0f;
        minValue = 0.0f;
        maxAngle = 30.0f;
        maxValue = 1.0f;
        factor = ( fabs( inclinationDifference ) - minAngle ) / ( maxAngle - minAngle );
        factor = ( factor < 0 ) ? 0 : ( ( factor > 1 ) ? 1 : factor );
        impulse = minValue * ( 1 - factor ) + maxValue * factor;
        if( inclinationDifference < 0 )
        {            
            spinalCord->down = impulse;
        }
        else
        {
            spinalCord->up = impulse;
        }

        // tracking modifier
        if( fabs( targetAngle ) < 90.0f && distanceToTarget > 2500.0f )
        {
            spinalCord->modifier = wingsuit;
        }
        else
        {
            spinalCord->modifier = false;
        }

        // force tracking in several condition        
        if( getNPC()->getCatToy()->getModifier() )
        {
            // not a skydiving?
            if( !getNPC()->getJumper()->getCanopySimulator()->getGearRecord()->skydiving )
            {
                // imitate cat toy modifier
                spinalCord->modifier = getNPC()->getCatToy()->getModifier();
            }
            else
            {
                Vector3f cattoyFaceH( catToyPose[2][0], 0, catToyPose[2][2] );
                cattoyFaceH.normalize();
                Vector3f npcFaceH = getNPC()->getJumper()->getClump()->getFrame()->getAt();
                npcFaceH[1] = 0;
                npcFaceH.normalize();
                //if( Vector3f::dot( dirH, cattoyFaceH ) > 0.85f ) spinalCord->modifier = true;
            }
        }
    }
}

bool NPCFreefall_DZ::isEndOfProgram(void)
{
    return getNPC()->getJumper()->getPhase() == ::jpFlight;
}

void NPCFreefall_DZ::onEndOfSubProgram(NPCProgram* subProgram)
{
}
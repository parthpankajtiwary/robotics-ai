#!/usr/bin/env python
import roslib;
import rospy
import actionlib

from alice_msgs.msg import *

if __name__ == '__main__':

    rospy.init_node('test_client')

    client = actionlib.SimpleActionClient('approach_objects', aliceapproachAction)
    client.wait_for_server()
    print 'found server'
	
    goal = aliceapproachGoal()
    #goal.plane = False
    
    client.send_goal(goal)
    client.wait_for_result(rospy.Duration.from_sec(60.0))  # use this for time-out checking, if takes to long stop waiting

    if (client.get_state() == actionlib.GoalStatus.ABORTED):  # aborted
        print 'aborted!' 
        result = client.get_result()

    elif (client.get_state() == actionlib.GoalStatus.SUCCEEDED):  # movement has succeeded
        print 'Succes!'
        result = client.get_result()

    elif (client.get_state() != actionlib.GoalStatus.SUCCEEDED): # most likely the time-out accord
        print 'Time out' 
        

    
    
	
	

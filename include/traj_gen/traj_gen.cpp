#include "traj_gen.hpp"
#include "convert_traj_to_target.hpp"

Traj_Generator::Traj_Generator()
{
    cout<<"Constructor is called.\n";

    // Get Yaml directory from launch file.
    nh_.getParam("yaml_config", yaml_dir);

    for(int i = 0; i < 3; i++)
        init_pos[i] = 0;


    nh_.getParam("init_pos3",init_pos[3]);
    nh_.getParam("init_pos4",init_pos[4]);
    nh_.getParam("init_pos5",init_pos[5]);
    
    nh_.getParam("init_inc0",init_inc[0]);
    nh_.getParam("init_inc1",init_inc[1]);
    nh_.getParam("init_inc2",init_inc[2]);
    nh_.getParam("init_inc3",init_inc[3]);
    nh_.getParam("init_inc4",init_inc[4]);
    nh_.getParam("init_inc5",init_inc[5]);

    for(int i = 0; i < 6; i++)
    {
        cout<<"Init pos["<<i<<"]: "<<init_pos[i]<<endl;
    }

    for(int i = 0; i < 6; i++)
    {
        cout<<"Init inc["<<i<<"]: "<<init_inc[i]<<endl;
    }

    for(int i = 0; i < 3; i++)
    {
        des_pos_STEERING[i] = 0;
        des_vel_WHEEL[i] = 0;
        goal_pos[i] = 0;
    }

    for(int i = 0; i < 6; i++)
    {
        traj_data.a_curr[i] = 0;
        traj_data.v_curr[i] = 0;
        traj_data.p_curr[i] = 0;
        des_pos[i] = 0;
    }

    mode_value = 255;


    // Allocate dynamic memory for yaml_read
    // and call the corresponding constructor.
    yaml_read_ptr = new YAML_READ(yaml_dir);

    constraint_setup();

    nh_mode_subscriber = nh_.subscribe("/mode_val",
                                        10,
                                        &Traj_Generator::callbackModeVal,
                                        this);
    
    nh_motors_publisher = nh_.advertise<target>("/target", 10);

    nh_dxl_publisher = nh_.advertise<target_dxl>("/target_dxl",10);

}

void Traj_Generator::constraint_setup()
{
    yaml_read_ptr->YamlLoadFile();
    for(int i = 0; i < 2; i++)
        traj_constraint[i] = yaml_read_ptr->traj_constraint[i];
    
    // Get trajectory constraints
    for(int i = 0; i < 2; i++)
    {
        cout<<"Name: "<<traj_constraint[i].name<<endl;
        cout<<"a_max: "<<traj_constraint[i].a_max<<endl;
        cout<<"v_max: "<<traj_constraint[i].v_max<<endl;
    }
    // Allocate dynamic memory for dip
    // and call the corresponding constructor.
    for(int i = 0; i < 6; i++)
        dip_ptr[i] = new DoubleIntegralPlanner(
            traj_constraint[i/3].a_max, 
            traj_constraint[i/3].v_max);

}

void Traj_Generator::callbackModeVal(const mode::ConstPtr& mode_ref)
{
    for(int i = 0; i < 3; i++)
        goal_pos[i] = mode_ref->goal_pos[i];

    mode_value = mode_ref->mode_val;

    if(mode_value == 1)
    {
        set_DXL_BEFORE_LIFT();
    }
    else if(mode_value == 2)
    {
        set_LIFT();
    }
    else if(mode_value == 3)
    {
        set_DXL_BEFORE_PAN();
    }
    else if(mode_value == 4)
    {
        set_PAN();
    }

}

void Traj_Generator::callbackActual(const actual::ConstPtr& actual_ref)
{

}

void Traj_Generator::callbackTwist(const Twist::ConstPtr& twist_ref)
{

}

void Traj_Generator::set_DXL_BEFORE_LIFT()
{
    for(int i = 0; i < 3; i++)
        target_dxl_[i] = 2048;

}

void Traj_Generator::set_LIFT()
{
    for(int i = 0; i < 3; i++)
        setGoalPos(goal_pos[i] - init_pos[i+3], i+3);
    
    t_get_goal = ros::Time::now().toSec();
}

void Traj_Generator::set_DXL_BEFORE_PAN()
{
    for(int i = 0; i < 3; i++)
    {
        des_pos_STEERING[i] = 90.0;
        target_dxl_[i] = (int32_t)(des_pos_STEERING[i] * 4096.0/360.0 + 2048.0);
    }
}

void Traj_Generator::set_PAN()
{
    for(int i = 0; i < 3; i++)
        setGoalPos(goal_pos[i] - init_pos[i], i);

    t_get_goal = ros::Time::now().toSec();
}

void Traj_Generator::move_motors()
{
    if(mode_value == 1)
    {
        target_dxl_msg.stamp = ros::Time::now();
        for(int i = 0; i < 3; i++)
            target_dxl_msg.target_dxl[i] = target_dxl_[i];
    }
    else if(mode_value == 2)
    {
        t_traj = ros::Time::now().toSec() - t_get_goal - 2;
        cout<<"****************"<<endl;
        cout<<"Time: "<<t_traj<<endl;
        move_LIFT_motors();
        publish_target();
    }
    else if(mode_value == 3)
    {

    }
    else if(mode_value == 4)
    {
        t_traj = ros::Time::now().toSec() - t_get_goal - 2;
        cout<<"****************"<<endl;
        cout<<"Time: "<<t_traj<<endl;
        move_PAN_motors();
        publish_target();
    }
}

void Traj_Generator::publish_target()
{
    target_msg.stamp = ros::Time::now();
    for(int i = 0; i < 3; i++)
    {
        target_msg.target_PAN[i] = convert_deg2target_PAN(des_pos[i]) + init_inc[i];
        target_msg.target_LIFT[i] = convert_deg2target_LIFT(des_pos[i+3]) + init_inc[i+3];
    }
    nh_motors_publisher.publish(target_msg);
}

void Traj_Generator::move_PAN_motors()
{
    for(int i = 0; i < 3; i++)
        getTraj(traj_data, i, t_traj);
    
    init_des_traj(0);

    for(int i = 0; i < 3; i++)
        des_pos[i+3] = init_pos[i+3];

    for(int i = 0 ; i < 6; i++)
        cout<<"Motor ["<<i<<"] : "<<des_pos[i]<<endl;
}

void Traj_Generator::move_LIFT_motors()
{
    for(int i = 0; i < 3; i++)
        getTraj(traj_data, i+3, t_traj);

    init_des_traj(3);

    for(int i = 0; i < 3; i++)
        des_pos[i] = init_pos[i];

    for(int i = 0 ; i < 6; i++)
        cout<<"Motor ["<<i<<"] : "<<des_pos[i]<<endl;
}

void Traj_Generator::init_des_traj(int offset)
{
    for(int i = 0; i < 3; i++)
    {
        if(t_traj > dip_ptr[i+offset]->getFinalTime() + 1)
        {
            init_pos[i+offset] = des_pos[i+offset];
        }
        else
        {
            des_pos[i+offset] = traj_data.p_curr[i+offset] + init_pos[i+offset];
        }
    }
}

void Traj_Generator::setGoalPos(double goal_pos, int idx)
{
    dip_ptr[idx]->setGoalPos(goal_pos);
    dip_ptr[idx]->TimeCalc();
}

void Traj_Generator::getTraj(TRJ_DATA &traj_ref, int idx, double time)
{
    dip_ptr[idx]->goalTraj(time);
    traj_ref.p_curr[idx] = dip_ptr[idx]->getPos();
    traj_ref.v_curr[idx] = dip_ptr[idx]->getVel();

}

Traj_Generator::~Traj_Generator()
{
    // Delete the allocated dynamic memory for dip
    for(int i = 0; i < 6; i++)
        delete dip_ptr[i];

    // Delete the allocated dynamic memory for yaml_read
    delete yaml_read_ptr;

    cout<<"Destructor is called."<<endl;
}
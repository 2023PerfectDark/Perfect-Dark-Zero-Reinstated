    static thread_local landing_motion::State landing;
    static thread_local uint32_t landing_target=0;
    const bool switched=landing_target!=target;
    landing_target=target;
    const float dip=landing.Step(camera_dt,LoadBeFloat(base,target+52),first_person,
                                switched||distance>100.f);
    // Translate eye and look point together: aim direction stays unchanged.
    // The native camera rebuilds both next frame, so this never accumulates.
    if(dip>0){
      StoreBeFloat(base,mode+kCameraPosition+4,LoadBeFloat(base,mode+kCameraPosition+4)-dip);
      StoreBeFloat(base,mode+kLookPoint+4,LoadBeFloat(base,mode+kLookPoint+4)-dip);
    }

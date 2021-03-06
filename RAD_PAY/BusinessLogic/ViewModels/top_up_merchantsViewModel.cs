﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class top_up_merchantsViewModel
    {
        public int id { get; set; }
        public string name { get; set; }
        public int? status { get; set; }
        public int? option { get; set; }
        public long? min_amount { get; set; }
        public long? max_amount { get; set; }
        public long? rate { get; set; }
        public int? position { get; set; }
        public long? card_id { get; set; }
        public long? icon_id { get; set; }
    }
}
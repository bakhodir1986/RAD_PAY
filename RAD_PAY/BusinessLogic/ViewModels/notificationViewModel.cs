using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class notificationViewModel
    {
        public int id { get; set; }
        public string msg { get; set; }
        public DateTime? add_ts { get; set; }
        public int? send { get; set; }
        public long? uid { get; set; }
        public string type { get; set; }
    }
}
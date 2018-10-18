using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class card_monitoring_tarifViewModel
    {
        public long id { get; set; }
        public long? amount { get; set; }
        public int? mid { get; set; }
        public int? status { get; set; }
    }
}